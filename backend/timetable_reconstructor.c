
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "sqlite3.h"

#define DB_PATH "database/roomsync.db"
#define MAX_ROOMS 256
#define MAX_TEXT 512
#define DAYS 5
#define SLOTS 9

typedef struct {
    int id;
    char number[64];
} Room;

typedef struct {
    int y;
} Anchor;

static const char *dayNames[DAYS] = {
    "MON", "TUE", "WED", "THU", "FRI"
};

static const int pdf01Centers[SLOTS] = {
    568, 814, 1090, 1490, 1745,
    2070, 2400, 2780, 3190
};

static const char *startTimes[SLOTS] = {
    "09:00", "10:00", "11:00",
    "12:00", "13:00", "14:00",
    "15:00", "16:00", "17:00"
};

static const char *endTimes[SLOTS] = {
    "10:00", "11:00", "12:00",
    "13:00", "14:00", "15:00",
    "16:00", "17:00", "18:00"
};

void normalize(const char *src, char *dst, int size) {
    int j = 0;

    for (int i = 0; src[i] && j < size - 1; i++) {
        if (isalnum((unsigned char)src[i]))
            dst[j++] = (char)toupper((unsigned char)src[i]);
    }

    dst[j] = '\0';
}

int detectDay(const char *text) {
    char t[MAX_TEXT];
    normalize(text, t, sizeof(t));

    if (!strcmp(t, "MON") || !strcmp(t, "MONDAY")) return 0;
    if (!strcmp(t, "TUE") || !strcmp(t, "TUES") ||
        !strcmp(t, "TUESDAY")) return 1;
    if (!strcmp(t, "WED") || !strcmp(t, "WEDNESDAY")) return 2;
    if (!strcmp(t, "THU") || !strcmp(t, "THUR") ||
        !strcmp(t, "THURS") || !strcmp(t, "THURSDAY")) return 3;
    if (!strcmp(t, "FRI") || !strcmp(t, "FRIDAY")) return 4;

    return -1;
}

/*
   Match a known room number anywhere inside a combined token.

   Examples:
   SwV/CB613             -> CB613
   /CME505               -> CME505
   LAB/DSC2151/...CME604 -> CME604

   Only room numbers already present in the database are accepted.
*/
int matchesRoom(const char *text, const char *room) {
    if (!text || !room || !room[0])
        return 0;

    int n = (int)strlen(room);
    int textLength = (int)strlen(text);

    if (n == 0 || textLength < n)
        return 0;

    for (int i = 0; i <= textLength - n; i++) {
        if (strncasecmp(text + i, room, n) == 0)
            return 1;
    }

    return 0;
}

int loadRooms(sqlite3 *db, Room rooms[]) {
    sqlite3_stmt *s = NULL;

    const char *sql =
        "SELECT room_id,room_number FROM rooms "
        "WHERE room_number IS NOT NULL "
        "AND TRIM(room_number) <> '' "
        "ORDER BY room_number";

    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK)
        return -1;

    int count = 0;

    while (sqlite3_step(s) == SQLITE_ROW && count < MAX_ROOMS) {
        rooms[count].id = sqlite3_column_int(s, 0);

        snprintf(rooms[count].number,
                 sizeof(rooms[count].number),
                 "%s",
                 (const char *)sqlite3_column_text(s, 1));

        count++;
    }

    sqlite3_finalize(s);
    return count;
}

int createTable(sqlite3 *db) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS timetable_room_candidates ("
        "candidate_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pdf_id TEXT NOT NULL,"
        "department_id INTEGER NOT NULL,"
        "year INTEGER NOT NULL,"
        "source_page INTEGER NOT NULL,"
        "room_id INTEGER NOT NULL,"
        "room_number TEXT NOT NULL,"
        "day TEXT,"
        "start_time TEXT,"
        "end_time TEXT,"
        "left_pos INTEGER NOT NULL,"
        "top_pos INTEGER NOT NULL,"
        "ocr_text TEXT NOT NULL,"
        "reconstruction_status TEXT NOT NULL,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE(pdf_id,source_page,room_id,left_pos,top_pos,ocr_text)"
        ");";

    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

int getAnchors(sqlite3 *db, const char *pdf,
               int page, Anchor anchors[]) {
    sqlite3_stmt *s = NULL;

    const char *sql =
        "SELECT text,top_pos FROM timetable_ocr_words "
        "WHERE pdf_id=? AND source_page=? "
        "ORDER BY top_pos";

    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(s, 1, pdf, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 2, page);

    int found[DAYS] = {0};

    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *t =
            (const char *)sqlite3_column_text(s, 0);

        int d = detectDay(t ? t : "");

        if (d >= 0 && !found[d]) {
            found[d] = 1;
            anchors[d].y = sqlite3_column_int(s, 1);
        }
    }

    sqlite3_finalize(s);

    for (int i = 0; i < DAYS; i++) {
        if (!found[i])
            return 0;
    }

    for (int i = 0; i < DAYS - 1; i++) {
        if (anchors[i].y >= anchors[i + 1].y)
            return 0;
    }

    return 1;
}

const char *getDay(Anchor anchors[], int y) {
    for (int i = 0; i < DAYS - 1; i++) {
        int boundary =
            (anchors[i].y + anchors[i + 1].y) / 2;

        if (y < boundary)
            return dayNames[i];
    }

    return dayNames[4];
}

int getSlot(const char *pdf, int x) {
    if (strcmp(pdf, "pdf01") != 0)
        return -1;

    int best = -1;
    int distance = 999999;

    for (int i = 0; i < SLOTS; i++) {
        int d = abs(x - pdf01Centers[i]);

        if (d < distance) {
            distance = d;
            best = i;
        }
    }

    if (distance > 110)
        return -1;

    return best;
}

int main(void) {
    sqlite3 *db = NULL;
    Room rooms[MAX_ROOMS];

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        printf("Database open failed.\n");
        if (db) sqlite3_close(db);
        return 1;
    }

    int roomCount = loadRooms(db, rooms);

    if (roomCount < 0) {
        printf("Could not load rooms: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    if (!createTable(db)) {
        printf("Candidate table creation failed: %s\n",
               sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *pageSQL =
        "SELECT DISTINCT pdf_id,department_id,year,source_page "
        "FROM timetable_ocr_words "
        "ORDER BY pdf_id,source_page";

    sqlite3_stmt *pages = NULL;

    if (sqlite3_prepare_v2(db, pageSQL, -1,
                           &pages, NULL) != SQLITE_OK) {
        printf("Page query failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *wordSQL =
        "SELECT text,left_pos,top_pos "
        "FROM timetable_ocr_words "
        "WHERE pdf_id=? AND source_page=? "
        "ORDER BY top_pos,left_pos";

    sqlite3_stmt *words = NULL;

    if (sqlite3_prepare_v2(db, wordSQL, -1,
                           &words, NULL) != SQLITE_OK) {
        printf("Word query failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(pages);
        sqlite3_close(db);
        return 1;
    }

    /*
       Update matching existing candidates as well.
       The live routines table is never modified.
    */
    const char *insertSQL =
        "INSERT INTO timetable_room_candidates "
        "(pdf_id,department_id,year,source_page,room_id,"
        "room_number,day,start_time,end_time,left_pos,top_pos,"
        "ocr_text,reconstruction_status) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(pdf_id,source_page,room_id,left_pos,top_pos,ocr_text) "
        "DO UPDATE SET "
        "day=excluded.day,"
        "start_time=excluded.start_time,"
        "end_time=excluded.end_time,"
        "reconstruction_status=excluded.reconstruction_status;";

    sqlite3_stmt *insert = NULL;

    if (sqlite3_prepare_v2(db, insertSQL, -1,
                           &insert, NULL) != SQLITE_OK) {
        printf("Insert preparation failed: %s\n",
               sqlite3_errmsg(db));
        sqlite3_finalize(words);
        sqlite3_finalize(pages);
        sqlite3_close(db);
        return 1;
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE",
                     NULL, NULL, NULL) != SQLITE_OK) {
        printf("Transaction failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(insert);
        sqlite3_finalize(words);
        sqlite3_finalize(pages);
        sqlite3_close(db);
        return 1;
    }

    int pageCount = 0;
    int matchedWords = 0;
    int dayAssigned = 0;
    int dayUnresolved = 0;
    int timeAssigned = 0;
    int timeUnresolved = 0;

    while (sqlite3_step(pages) == SQLITE_ROW) {
        const char *p =
            (const char *)sqlite3_column_text(pages, 0);

        char pdf[64];
        snprintf(pdf, sizeof(pdf), "%s", p ? p : "");

        int dept = sqlite3_column_int(pages, 1);
        int year = sqlite3_column_int(pages, 2);
        int page = sqlite3_column_int(pages, 3);

        pageCount++;

        Anchor anchors[DAYS] = {{0}};
        int fullWeek = getAnchors(db, pdf, page, anchors);

        sqlite3_reset(words);
        sqlite3_clear_bindings(words);
        sqlite3_bind_text(words, 1, pdf, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(words, 2, page);

        while (sqlite3_step(words) == SQLITE_ROW) {
            const char *text =
                (const char *)sqlite3_column_text(words, 0);

            int x = sqlite3_column_int(words, 1);
            int y = sqlite3_column_int(words, 2);

            if (!text || !text[0])
                continue;

            for (int r = 0; r < roomCount; r++) {
                if (!matchesRoom(text, rooms[r].number))
                    continue;

                matchedWords++;

                const char *day = NULL;
                const char *start = NULL;
                const char *end = NULL;

                if (fullWeek) {
                    day = getDay(anchors, y);
                    dayAssigned++;
                } else {
                    dayUnresolved++;
                }

                int slot = getSlot(pdf, x);

                if (slot >= 0) {
                    start = startTimes[slot];
                    end = endTimes[slot];
                    timeAssigned++;
                } else {
                    timeUnresolved++;
                }

                const char *status;

                if (day && start && end)
                    status = "DAY_AND_TIME_ASSIGNED";
                else if (day)
                    status = "DAY_ASSIGNED_TIME_UNRESOLVED";
                else if (start)
                    status = "TIME_ASSIGNED_DAY_UNRESOLVED";
                else
                    status = "DAY_UNRESOLVED_TIME_UNRESOLVED";

                sqlite3_reset(insert);
                sqlite3_clear_bindings(insert);

                sqlite3_bind_text(insert, 1, pdf, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(insert, 2, dept);
                sqlite3_bind_int(insert, 3, year);
                sqlite3_bind_int(insert, 4, page);
                sqlite3_bind_int(insert, 5, rooms[r].id);
                sqlite3_bind_text(insert, 6, rooms[r].number,
                                  -1, SQLITE_TRANSIENT);

                if (day)
                    sqlite3_bind_text(insert, 7, day, -1,
                                      SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(insert, 7);

                if (start)
                    sqlite3_bind_text(insert, 8, start, -1,
                                      SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(insert, 8);

                if (end)
                    sqlite3_bind_text(insert, 9, end, -1,
                                      SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(insert, 9);

                sqlite3_bind_int(insert, 10, x);
                sqlite3_bind_int(insert, 11, y);
                sqlite3_bind_text(insert, 12, text, -1,
                                  SQLITE_TRANSIENT);
                sqlite3_bind_text(insert, 13, status, -1,
                                  SQLITE_TRANSIENT);

                if (sqlite3_step(insert) != SQLITE_DONE) {
                    printf("Candidate insert failed: %s\n",
                           sqlite3_errmsg(db));
                    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
                    sqlite3_finalize(insert);
                    sqlite3_finalize(words);
                    sqlite3_finalize(pages);
                    sqlite3_close(db);
                    return 1;
                }
            }
        }
    }

    sqlite3_finalize(insert);
    sqlite3_finalize(words);
    sqlite3_finalize(pages);

    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        printf("Commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_stmt *countStmt = NULL;
    int totalCandidates = 0;

    if (sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM timetable_room_candidates",
        -1, &countStmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(countStmt) == SQLITE_ROW)
            totalCandidates = sqlite3_column_int(countStmt, 0);
    }

    sqlite3_finalize(countStmt);

    printf("\nRoomSync embedded-room reconstruction completed.\n");
    printf("Rooms in inventory       : %d\n", roomCount);
    printf("OCR pages examined       : %d\n", pageCount);
    printf("Room matches found       : %d\n", matchedWords);
    printf("Total staged candidates  : %d\n", totalCandidates);
    printf("Day assigned             : %d\n", dayAssigned);
    printf("Day unresolved           : %d\n", dayUnresolved);
    printf("Time assigned            : %d\n", timeAssigned);
    printf("Time unresolved          : %d\n", timeUnresolved);
    printf("\nLive routines table was NOT modified.\n");

    sqlite3_close(db);
    return 0;
}
