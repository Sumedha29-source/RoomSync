
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sqlite3.h"

#define DB_PATH "database/roomsync.db"
#define TSV_DIR "timetable_extraction"
#define MAX_LINE 16384
#define MAX_PAGES 100

typedef struct {
    int department_id;
    int year;
} PdfInfo;

/* PDF ID -> department ID and year.
   Department IDs match the existing RoomSync database. */
static const PdfInfo pdf_info[46] = {
    {0, 0},
    {4,1}, {4,2}, {4,3}, {4,4},
    {5,3}, {5,4},
    {6,2}, {6,3}, {6,4},
    {7,2}, {7,3}, {7,4},
    {2,2}, {2,3}, {2,4},
    {8,2}, {8,3}, {8,4},
    {1,3}, {1,4},
    {9,1},
    {10,1}, {10,2}, {10,3}, {10,4},
    {11,1}, {11,2}, {11,3}, {11,4},
    {3,1}, {3,2}, {3,3}, {3,4},
    {12,1}, {12,2}, {12,3}, {12,4},
    {13,1}, {13,2}, {13,3}, {13,4},
    {14,1}, {14,2}, {14,3}, {14,4}
};

/* Split a TSV row while preserving empty columns. */
static int splitTabs(char *line, char **fields, int maxFields)
{
    int count = 0;
    char *start = line;

    while (count < maxFields) {
        fields[count++] = start;

        char *tab = strchr(start, '\t');
        if (!tab)
            break;

        *tab = '\0';
        start = tab + 1;
    }

    return count;
}

static void removeNewline(char *text)
{
    text[strcspn(text, "\r\n")] = '\0';
}

static int createStagingTable(sqlite3 *db)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS timetable_ocr_words ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pdf_id TEXT NOT NULL,"
        "department_id INTEGER NOT NULL,"
        "year INTEGER NOT NULL,"
        "source_page INTEGER NOT NULL,"
        "ocr_page_num INTEGER,"
        "block_num INTEGER,"
        "par_num INTEGER,"
        "line_num INTEGER,"
        "word_num INTEGER,"
        "left_pos INTEGER,"
        "top_pos INTEGER,"
        "word_width INTEGER,"
        "word_height INTEGER,"
        "confidence REAL,"
        "text TEXT NOT NULL,"
        "imported_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE(pdf_id, source_page, block_num, par_num, line_num, word_num)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_ocr_pdf_page "
        "ON timetable_ocr_words(pdf_id, source_page);"
        "CREATE INDEX IF NOT EXISTS idx_ocr_coordinates "
        "ON timetable_ocr_words(pdf_id, source_page, top_pos, left_pos);";

    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Could not create staging table: %s\n",
                error ? error : sqlite3_errmsg(db));
        sqlite3_free(error);
        return 0;
    }

    return 1;
}

static int importPage(
    sqlite3 *db,
    sqlite3_stmt *stmt,
    int pdfNumber,
    int pageNumber,
    int departmentId,
    int year,
    long long *inserted,
    long long *skipped)
{
    char path[300];
    char pdfId[16];
    char line[MAX_LINE];

    snprintf(pdfId, sizeof(pdfId), "pdf%02d", pdfNumber);
    snprintf(path, sizeof(path), "%s/%s_%d.tsv",
             TSV_DIR, pdfId, pageNumber);

    FILE *file = fopen(path, "r");
    if (!file)
        return 0;

    /* Skip the TSV header. */
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        char *fields[12] = {0};

        removeNewline(line);

        if (splitTabs(line, fields, 12) < 12)
            continue;

        /* Tesseract level 5 = individual OCR word. */
        if (atoi(fields[0]) != 5)
            continue;

        if (!fields[11] || fields[11][0] == '\0')
            continue;

        int ocrPage = atoi(fields[1]);
        int block = atoi(fields[2]);
        int paragraph = atoi(fields[3]);
        int lineNumber = atoi(fields[4]);
        int wordNumber = atoi(fields[5]);
        int left = atoi(fields[6]);
        int top = atoi(fields[7]);
        int width = atoi(fields[8]);
        int height = atoi(fields[9]);
        double confidence = atof(fields[10]);

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        sqlite3_bind_text(stmt, 1, pdfId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, departmentId);
        sqlite3_bind_int(stmt, 3, year);
        sqlite3_bind_int(stmt, 4, pageNumber);
        sqlite3_bind_int(stmt, 5, ocrPage);
        sqlite3_bind_int(stmt, 6, block);
        sqlite3_bind_int(stmt, 7, paragraph);
        sqlite3_bind_int(stmt, 8, lineNumber);
        sqlite3_bind_int(stmt, 9, wordNumber);
        sqlite3_bind_int(stmt, 10, left);
        sqlite3_bind_int(stmt, 11, top);
        sqlite3_bind_int(stmt, 12, width);
        sqlite3_bind_int(stmt, 13, height);
        sqlite3_bind_double(stmt, 14, confidence);
        sqlite3_bind_text(stmt, 15, fields[11], -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);

        if (rc == SQLITE_DONE) {
            if (sqlite3_changes(db) > 0)
                (*inserted)++;
            else
                (*skipped)++;
        } else {
            fprintf(stderr, "Insert error in %s page %d: %s\n",
                    pdfId, pageNumber, sqlite3_errmsg(db));
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 1;
}

int main(void)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;

    long long inserted = 0;
    long long skipped = 0;
    int pagesFound = 0;
    int pdfsFound = 0;

    printf("RoomSync timetable OCR importer\n");
    printf("--------------------------------\n");

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n",
                db ? sqlite3_errmsg(db) : "SQLite error");
        if (db) sqlite3_close(db);
        return 1;
    }

    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

    if (!createStagingTable(db)) {
        sqlite3_close(db);
        return 1;
    }

    const char *insertSQL =
        "INSERT OR IGNORE INTO timetable_ocr_words ("
        "pdf_id, department_id, year, source_page, ocr_page_num,"
        "block_num, par_num, line_num, word_num,"
        "left_pos, top_pos, word_width, word_height, confidence, text"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Cannot prepare insert: %s\n",
                sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Cannot begin transaction: %s\n",
                sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    for (int pdf = 1; pdf <= 45; pdf++) {
        int departmentId = pdf_info[pdf].department_id;
        int year = pdf_info[pdf].year;
        int foundForPdf = 0;

        for (int page = 1; page <= MAX_PAGES; page++) {
            int result = importPage(
                db, stmt, pdf, page,
                departmentId, year,
                &inserted, &skipped
            );

            if (result < 0) {
                sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return 1;
            }

            if (result > 0) {
                foundForPdf++;
                pagesFound++;
            }
        }

        if (foundForPdf > 0) {
            pdfsFound++;
            printf("pdf%02d: %d page(s) imported\n",
                   pdf, foundForPdf);
        }
    }

    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "Commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_finalize(stmt);

    printf("\nImport completed.\n");
    printf("PDFs with extracted pages : %d\n", pdfsFound);
    printf("TSV pages found            : %d\n", pagesFound);
    printf("New OCR words inserted     : %lld\n", inserted);
    printf("Existing words skipped     : %lld\n", skipped);

    sqlite3_close(db);
    return 0;
}
