
#include <stdio.h>
#include <sqlite3.h>

#define DB_PATH "database/roomsync.db"
#define BACKUP_PATH "database/roomsync_before_partial_publish.db"

int main(void) {
    sqlite3 *db = NULL;
    sqlite3 *backupDB = NULL;
    sqlite3_backup *backup = NULL;
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        printf("Database open failed: %s\n", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return 1;
    }

    /* Back up the database before publishing anything. */
    if (sqlite3_open(BACKUP_PATH, &backupDB) != SQLITE_OK) {
        printf("Backup database could not be opened.\n");
        sqlite3_close(db);
        if (backupDB) sqlite3_close(backupDB);
        return 1;
    }

    backup = sqlite3_backup_init(backupDB, "main", db, "main");

    if (!backup || sqlite3_backup_step(backup, -1) != SQLITE_DONE) {
        printf("Database backup failed.\n");
        if (backup) sqlite3_backup_finish(backup);
        sqlite3_close(backupDB);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_backup_finish(backup);
    sqlite3_close(backupDB);

    printf("Database backup created.\n");

    const char *sql =
        "BEGIN IMMEDIATE;"

        "INSERT INTO routines "
        "(department_id, year, section, day, start_time, end_time, room_id, subject) "

        "SELECT DISTINCT "
        "c.department_id, c.year, 'ALL', "
       "c.day, c.start_time, c.end_time, c.room_id, 'Subject not extracted' "

        "FROM timetable_room_candidates c "

        "WHERE c.day IS NOT NULL "
        "AND c.start_time IS NOT NULL "
        "AND c.end_time IS NOT NULL "
        "AND TRIM(c.day) <> '' "
        "AND TRIM(c.start_time) <> '' "
        "AND TRIM(c.end_time) <> '' "

        "AND NOT EXISTS ("
        " SELECT 1 FROM routines r "
        " WHERE r.department_id = c.department_id "
        " AND r.year = c.year "
        " AND r.day = c.day "
        " AND r.start_time = c.start_time "
        " AND r.end_time = c.end_time "
        " AND r.room_id = c.room_id"
        ");";

    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        printf("Publishing failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return 1;
    }

    int inserted = sqlite3_changes(db);

    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        printf("Commit failed: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return 1;
    }

    printf("\nPartial timetable publishing completed.\n");
    printf("New routine entries: %d\n", inserted);

    if (inserted == 0) {
        printf("No candidates currently have both day and time assigned.\n");
    }

    sqlite3_close(db);
    return 0;
}
