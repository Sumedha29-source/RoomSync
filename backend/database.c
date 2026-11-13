
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <bcrypt.h>
#include "database.h"

#define SALT_SIZE 16
#define HASH_SIZE 32
#define ITERATIONS 120000

/* =========================================
   GENERAL HELPERS
   ========================================= */

static void setError(char *error, int size, const char *message)
{
    if (error && size > 0)
        snprintf(error, size, "%s", message);
}

int executeSQL(sqlite3 *db, const char *sql)
{
    char *error = NULL;

    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);

    if (rc != SQLITE_OK)
    {
        printf("Database error: %s\n",
               error ? error : sqlite3_errmsg(db));

        sqlite3_free(error);
        return 0;
    }

    return 1;
}

/* =========================================
   DATABASE INITIALIZATION
   ========================================= */

static int columnExists(sqlite3 *db,
                        const char *table,
                        const char *column)
{
    sqlite3_stmt *stmt = NULL;
    char sql[128];

    snprintf(sql, sizeof(sql),
             "PRAGMA table_info(%s);", table);

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *name =
            (const char *)sqlite3_column_text(stmt, 1);

        if (name && strcmp(name, column) == 0)
        {
            sqlite3_finalize(stmt);
            return 1;
        }
    }

    sqlite3_finalize(stmt);
    return 0;
}


static int initializeDatabase(sqlite3 *db)
{
    /* Add building column to existing rooms table if needed */
    if (!columnExists(db, "rooms", "building"))
    {
        if (!executeSQL(db,
            "ALTER TABLE rooms ADD COLUMN building TEXT;"))
            return 0;
    }

    /* Create users table */
    if (!executeSQL(db,
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password_hash TEXT NOT NULL,"
        "salt TEXT NOT NULL,"
        "role TEXT NOT NULL CHECK(role IN ('ADMIN','MANAGER')),"
        "must_change_password INTEGER DEFAULT 0"
        ");"))
        return 0;

    /* Add password-change column to an older users table */
    if (!columnExists(db, "users", "must_change_password"))
    {
        if (!executeSQL(db,
            "ALTER TABLE users ADD COLUMN "
            "must_change_password INTEGER DEFAULT 0;"))
            return 0;
    }

    /* Create manager assignments table */
    if (!executeSQL(db,
        "CREATE TABLE IF NOT EXISTS manager_assignments ("
        "username TEXT NOT NULL,"
        "building TEXT NOT NULL,"
        "floor INTEGER NOT NULL,"
        "UNIQUE(username, building, floor)"
        ");"))
        return 0;

    return 1;
}
/* =========================================
   DATABASE CONNECTION
   ========================================= */

sqlite3 *openDatabase(void)
{
    sqlite3 *db = NULL;

    int rc = sqlite3_open("database/roomsync.db", &db);

    if (rc != SQLITE_OK)
    {
        printf("Cannot open database: %s\n",
               sqlite3_errmsg(db));

        sqlite3_close(db);
        return NULL;
    }

    if (!initializeDatabase(db))
    {
        printf("Database initialization failed.\n");
        sqlite3_close(db);
        return NULL;
    }

    printf("Database connected successfully.\n");

    return db;
}

void closeDatabase(sqlite3 *db)
{
    if (db != NULL)
    {
        sqlite3_close(db);
    }
}

/* =========================================
   ROOM VALIDATION
   ========================================= */

static int validBuilding(const char *building)
{
    return building &&
        (strcmp(building, "ICT") == 0 ||
         strcmp(building, "CME") == 0 ||
         strcmp(building, "CB") == 0);
}

static int validRoomFields(const char *roomName,
                           const char *building,
                           int floor,
                           int capacity,
                           char *error,
                           int errorSize)
{
    size_t i, length;

    if (!roomName || !building)
    {
        setError(error, errorSize, "Room name and building are required.");
        return 0;
    }

    if (!validBuilding(building))
    {
        setError(error, errorSize, "Select ICT, CME or CB building.");
        return 0;
    }

    length = strlen(roomName);

    if (length < 4 || length > 19 ||
        strncmp(roomName, building, strlen(building)) != 0)
    {
        setError(error, errorSize,
                 "Room name must begin with its building name, e.g. ICT101.");
        return 0;
    }

    for (i = strlen(building); i < length; i++)
    {
        if (!isdigit((unsigned char)roomName[i]))
        {
            setError(error, errorSize,
                     "Room name must contain the building name followed by numbers.");
            return 0;
        }
    }

    if (floor < 0 || floor > 100)
    {
        setError(error, errorSize, "Invalid floor number.");
        return 0;
    }

    if (capacity < 1 || capacity > 1000)
    {
        setError(error, errorSize, "Capacity must be between 1 and 1000.");
        return 0;
    }

    return 1;
}

/* =========================================
   JSON HELPERS
   ========================================= */

static int appendJSON(char *output,
                      int outputSize,
                      int *used,
                      const char *format,
                      const char *roomName,
                      const char *building,
                      int floor,
                      int capacity,
                      const char *status)
{
    int written;

    if (*used >= outputSize)
        return 0;

    written = snprintf(
        output + *used,
        (size_t)(outputSize - *used),
        format,
        roomName,
        building,
        floor,
        capacity,
        status
    );

    if (written < 0 || written >= outputSize - *used)
        return 0;

    *used += written;

    return 1;
}

/* =========================================
   GET ALL ROOMS
   ========================================= */

int getAllRoomsJSON(sqlite3 *db,
                    char *output,
                    int outputSize)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "SELECT room_number, building, floor, capacity, status "
        "FROM rooms ORDER BY building, floor, room_number;";

    int rc, used = 0, first = 1;

    if (!output || outputSize < 3)
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    output[0] = '[';
    output[1] = '\0';
    used = 1;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *room =
            (const char *)sqlite3_column_text(stmt, 0);

        const char *building =
            (const char *)sqlite3_column_text(stmt, 1);

        const char *status =
            (const char *)sqlite3_column_text(stmt, 4);

        if (!first)
        {
            if (used >= outputSize - 1)
            {
                sqlite3_finalize(stmt);
                return 0;
            }

            output[used++] = ',';
            output[used] = '\0';
        }

        if (!appendJSON(
            output, outputSize, &used,
            "{\"room_number\":\"%s\",\"building\":\"%s\","
            "\"floor\":%d,\"capacity\":%d,\"status\":\"%s\"}",
            room ? room : "",
            building ? building : "",
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            status ? status : "EMPTY"))
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        first = 0;
    }

    if (rc != SQLITE_DONE || used >= outputSize - 1)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    output[used++] = ']';
    output[used] = '\0';

    sqlite3_finalize(stmt);

    return 1;
}


/* =========================================
   MANAGER ASSIGNED ROOMS
   ========================================= */



int getManagerRoomsJSON(sqlite3 *db,
                         const char *username,
                         char *output,
                         int outputSize)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "SELECT r.room_number, r.building, r.floor, "
        "r.capacity, r.status, "
        "r.current_occupancy, r.status_updated_at "
        "FROM rooms r "
        "INNER JOIN manager_assignments m "
        "ON r.building = m.building "
        "AND r.floor = m.floor "
        "WHERE m.username = ? "
        "ORDER BY r.building, r.floor, r.room_number;";

    int rc, used = 1, first = 1;

    if (!db || !username || !output || outputSize < 3)
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    output[0] = '[';
    output[1] = '\0';

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *room =
            (const char *)sqlite3_column_text(stmt, 0);

        const char *building =
            (const char *)sqlite3_column_text(stmt, 1);

        const char *status =
            (const char *)sqlite3_column_text(stmt, 4);

        const char *updatedAt =
            (const char *)sqlite3_column_text(stmt, 6);

        int written;

        if (!first)
        {
            if (used >= outputSize - 1)
            {
                sqlite3_finalize(stmt);
                return 0;
            }

            output[used++] = ',';
            output[used] = '\0';
        }

        written = snprintf(
            output + used,
            (size_t)(outputSize - used),
            "{\"room_number\":\"%s\",\"building\":\"%s\","
            "\"floor\":%d,\"capacity\":%d,\"status\":\"%s\","
            "\"current_occupancy\":",
            room ? room : "",
            building ? building : "",
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            status ? status : "EMPTY"
        );

        if (written < 0 || written >= outputSize - used)
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        used += written;

        if (sqlite3_column_type(stmt, 5) == SQLITE_NULL)
        {
            written = snprintf(output + used,
                               (size_t)(outputSize - used),
                               "null");
        }
        else
        {
            written = snprintf(output + used,
                               (size_t)(outputSize - used),
                               "%d",
                               sqlite3_column_int(stmt, 5));
        }

        if (written < 0 || written >= outputSize - used)
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        used += written;

        if (sqlite3_column_type(stmt, 6) == SQLITE_NULL)
        {
            written = snprintf(output + used,
                               (size_t)(outputSize - used),
                               ",\"status_updated_at\":null}");
        }
        else
        {
            written = snprintf(output + used,
                               (size_t)(outputSize - used),
                               ",\"status_updated_at\":\"%s\"}",
                               updatedAt ? updatedAt : "");
        }

        if (written < 0 || written >= outputSize - used)
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        used += written;
        first = 0;
    }

    if (rc != SQLITE_DONE || used >= outputSize - 1)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    output[used++] = ']';
    output[used] = '\0';

    sqlite3_finalize(stmt);

    return 1;
}

/* =========================================
   AVAILABLE ROOMS
   ========================================= */


int getAvailableRoomsJSON(sqlite3 *db,
                          const char *day,
                          const char *startTime,
                          const char *endTime,
                          char *output,
                          int outputSize)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "SELECT "
        "r.room_number, "
        "r.building, "
        "r.floor, "
        "r.capacity, "
        "CASE WHEN EXISTS ("
        "SELECT 1 FROM routines t "
        "WHERE t.room_id = r.room_id "
        "AND t.day = ? "
        "AND t.start_time < ? "
        "AND t.end_time > ?"
        ") THEN 'OCCUPIED' ELSE 'EMPTY' END, "
        "CASE WHEN r.status_updated_at IS NULL "
        "OR r.current_occupancy IS NULL "
        "THEN 'NOT_CHECKED' ELSE r.status END, "
        "r.current_occupancy, "
        "r.status_updated_at "
        "FROM rooms r "
        "ORDER BY r.building, r.floor, r.room_number;";

    int rc;
    int used = 1;
    int first = 1;

    if (!output || outputSize < 3)
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, day, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, endTime, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, startTime, -1, SQLITE_TRANSIENT);

    output[0] = '[';
    output[1] = '\0';

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *room =
            (const char *)sqlite3_column_text(stmt, 0);

        const char *building =
            (const char *)sqlite3_column_text(stmt, 1);

        const char *expectedStatus =
            (const char *)sqlite3_column_text(stmt, 4);

        const char *actualStatus =
            (const char *)sqlite3_column_text(stmt, 5);

        const char *checkedAt =
            (const char *)sqlite3_column_text(stmt, 7);

        char roomJSON[1024];

        int floor = sqlite3_column_int(stmt, 2);
        int capacity = sqlite3_column_int(stmt, 3);

        int occupancyIsNull =
            sqlite3_column_type(stmt, 6) == SQLITE_NULL;

        int timestampIsNull =
            sqlite3_column_type(stmt, 7) == SQLITE_NULL;

        int occupancy =
            occupancyIsNull ? 0 : sqlite3_column_int(stmt, 6);

        int written;

        if (!room || !expectedStatus || !actualStatus)
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        if (first)
        {
            first = 0;
        }
        else
        {
            if (used >= outputSize - 1)
            {
                sqlite3_finalize(stmt);
                return 0;
            }

            output[used++] = ',';
            output[used] = '\0';
        }

        if (occupancyIsNull)
        {
            written = snprintf(
                roomJSON, sizeof(roomJSON),
                "{\"room_number\":\"%s\",\"building\":\"%s\","
                "\"floor\":%d,\"capacity\":%d,"
                "\"expected_status\":\"%s\","
                "\"status\":\"%s\","
                "\"current_occupancy\":null,"
                "\"status_updated_at\":null}",
                room,
                building ? building : "",
                floor,
                capacity,
                expectedStatus,
                actualStatus
            );
        }
        else if (timestampIsNull)
        {
            written = snprintf(
                roomJSON, sizeof(roomJSON),
                "{\"room_number\":\"%s\",\"building\":\"%s\","
                "\"floor\":%d,\"capacity\":%d,"
                "\"expected_status\":\"%s\","
                "\"status\":\"%s\","
                "\"current_occupancy\":%d,"
                "\"status_updated_at\":null}",
                room,
                building ? building : "",
                floor,
                capacity,
                expectedStatus,
                actualStatus,
                occupancy
            );
        }
        else
        {
            written = snprintf(
                roomJSON, sizeof(roomJSON),
                "{\"room_number\":\"%s\",\"building\":\"%s\","
                "\"floor\":%d,\"capacity\":%d,"
                "\"expected_status\":\"%s\","
                "\"status\":\"%s\","
                "\"current_occupancy\":%d,"
                "\"status_updated_at\":\"%s\"}",
                room,
                building ? building : "",
                floor,
                capacity,
                expectedStatus,
                actualStatus,
                occupancy,
                checkedAt ? checkedAt : ""
            );
        }

        if (written < 0 ||
            written >= (int)sizeof(roomJSON) ||
            written >= outputSize - used)
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        memcpy(output + used, roomJSON, written);
        used += written;
        output[used] = '\0';
    }

    if (rc != SQLITE_DONE || used >= outputSize - 1)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    output[used++] = ']';
    output[used] = '\0';

    sqlite3_finalize(stmt);

    return 1;
}


/* =========================================
   UPDATE ROOM STATUS
   ========================================= */


int updateRoomStatus(sqlite3 *db,
                     const char *roomName,
                     const char *status,
                     int occupancy)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "UPDATE rooms SET "
        "status = ?, "
        "current_occupancy = ?, "
        "status_updated_at = datetime('now') "
        "WHERE room_number = ? "
        "AND capacity >= ?;";

    if (!roomName || !status || occupancy < 0)
        return 0;

    if (strcmp(status, "EMPTY") != 0 &&
        strcmp(status, "OCCUPIED") != 0)
        return 0;

    if ((strcmp(status, "EMPTY") == 0 && occupancy != 0) ||
        (strcmp(status, "OCCUPIED") == 0 && occupancy == 0))
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, occupancy);
    sqlite3_bind_text(stmt, 3, roomName, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, occupancy);

    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);

    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE && changed > 0;
}

/* =========================================
   ADD ROOM
   ========================================= */

int addRoom(sqlite3 *db,
            const char *roomName,
            const char *building,
            int floor,
            int capacity,
            char *error,
            int errorSize)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "INSERT INTO rooms "
        "(room_number, building, floor, capacity, status) "
        "VALUES (?, ?, ?, ?, 'EMPTY');";

    if (!validRoomFields(roomName, building,
                         floor, capacity, error, errorSize))
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, roomName, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, building, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, floor);
    sqlite3_bind_int(stmt, 4, capacity);

    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE)
    {
        if (sqlite3_errcode(db) == SQLITE_CONSTRAINT)
            setError(error, errorSize, "This room name already exists.");
        else
            setError(error, errorSize, sqlite3_errmsg(db));

        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);

    setError(error, errorSize, "Room added successfully.");

    return 1;
}

/* =========================================
   EDIT ROOM
   ========================================= */

int editRoom(sqlite3 *db,
             const char *oldRoomName,
             const char *newRoomName,
             const char *building,
             int floor,
             int capacity,
             char *error,
             int errorSize)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "UPDATE rooms SET room_number = ?, building = ?, "
        "floor = ?, capacity = ? WHERE room_number = ?;";

    if (!oldRoomName || !*oldRoomName)
    {
        setError(error, errorSize, "Original room name is required.");
        return 0;
    }

    if (!validRoomFields(newRoomName, building,
                         floor, capacity, error, errorSize))
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, newRoomName, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, building, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, floor);
    sqlite3_bind_int(stmt, 4, capacity);
    sqlite3_bind_text(stmt, 5, oldRoomName, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);

    if (rc != SQLITE_DONE || changed == 0)
    {
        setError(error, errorSize,
                 rc != SQLITE_DONE ? sqlite3_errmsg(db) :
                 "Room not found or no details changed.");

        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);

    setError(error, errorSize, "Room updated successfully.");

    return 1;
}

/* =========================================
   DELETE ROOM
   ========================================= */

int deleteRoom(sqlite3 *db,
               const char *roomName,
               char *error,
               int errorSize)
{
    sqlite3_stmt *stmt = NULL;
    int roomId;

    if (!roomName || !*roomName)
    {
        setError(error, errorSize, "Room name is required.");
        return 0;
    }

    if (sqlite3_prepare_v2(
        db,
        "SELECT room_id FROM rooms WHERE room_number = ?;",
        -1, &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, roomName, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        setError(error, errorSize, "Room not found.");
        return 0;
    }

    roomId = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (sqlite3_prepare_v2(
        db,
        "SELECT 1 FROM routines WHERE room_id = ? LIMIT 1;",
        -1, &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, roomId);

    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc == SQLITE_ROW)
    {
        setError(error, errorSize,
                 "This room is used in a timetable. Remove or reassign its routine entries first.");
        return 0;
    }

    if (rc != SQLITE_DONE)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    if (sqlite3_prepare_v2(
        db,
        "DELETE FROM rooms WHERE room_id = ?;",
        -1, &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, roomId);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    setError(error, errorSize, "Room deleted successfully.");

    return 1;
}

/* =========================================
   PASSWORD HASHING
   ========================================= */

static int generateSalt(unsigned char *salt)
{
    return BCryptGenRandom(
        NULL,
        salt,
        SALT_SIZE,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    ) == 0;
}

static int hashPassword(const char *password,
                        const unsigned char *salt,
                        unsigned char *hash)
{
    BCRYPT_ALG_HANDLE algorithm = NULL;

    NTSTATUS result;

    result = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        NULL,
        BCRYPT_ALG_HANDLE_HMAC_FLAG
    );

    if (result != 0)
        return 0;

    result = BCryptDeriveKeyPBKDF2(
        algorithm,
        (PUCHAR)password,
        (ULONG)strlen(password),
        (PUCHAR)salt,
        SALT_SIZE,
        ITERATIONS,
        hash,
        HASH_SIZE,
        0
    );

    BCryptCloseAlgorithmProvider(algorithm, 0);

    return result == 0;
}

static void bytesToHex(const unsigned char *bytes,
                       int length,
                       char *output)
{
    static const char hex[] = "0123456789abcdef";

    int i;

    for (i = 0; i < length; i++)
    {
        output[i * 2] = hex[bytes[i] >> 4];
        output[i * 2 + 1] = hex[bytes[i] & 15];
    }

    output[length * 2] = '\0';
}

static int hexToBytes(const char *hex,
                      unsigned char *output,
                      int length)
{
    int i;

    if (!hex || strlen(hex) != (size_t)(length * 2))
        return 0;

    for (i = 0; i < length; i++)
    {
        unsigned int value;

        if (sscanf(hex + i * 2, "%2x", &value) != 1)
            return 0;

        output[i] = (unsigned char)value;
    }

    return 1;
}

static int secureCompare(const unsigned char *a,
                         const unsigned char *b,
                         int length)
{
    unsigned char difference = 0;

    int i;

    for (i = 0; i < length; i++)
        difference |= a[i] ^ b[i];

    return difference == 0;
}


/* =========================================
   CREATE USER
   ========================================= */

int createUser(sqlite3 *db,
               const char *username,
               const char *password,
               const char *role,
               int mustChangePassword,
               char *error,
               int errorSize)
{
    sqlite3_stmt *stmt = NULL;

    unsigned char salt[SALT_SIZE];
    unsigned char hash[HASH_SIZE];

    char saltHex[SALT_SIZE * 2 + 1];
    char hashHex[HASH_SIZE * 2 + 1];

    const char *sql =
        "INSERT INTO users "
        "(username, password_hash, salt, role, must_change_password) "
        "VALUES (?, ?, ?, ?, ?);";

    if (!username || !*username || strlen(username) > 40 ||
        !password || strlen(password) < 8 ||
        !role ||
        (strcmp(role, "ADMIN") != 0 &&
         strcmp(role, "MANAGER") != 0) ||
        (mustChangePassword != 0 && mustChangePassword != 1))
    {
        setError(error, errorSize,
                 "Invalid username, password or role. Password must have at least 8 characters.");
        return 0;
    }

    if (!generateSalt(salt) ||
        !hashPassword(password, salt, hash))
    {
        setError(error, errorSize, "Password hashing failed.");
        return 0;
    }

    bytesToHex(salt, SALT_SIZE, saltHex);
    bytesToHex(hash, HASH_SIZE, hashHex);

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashHex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, saltHex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, mustChangePassword);

    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE)
    {
        setError(error, errorSize,
                 sqlite3_errcode(db) == SQLITE_CONSTRAINT ?
                 "Username already exists." :
                 sqlite3_errmsg(db));

        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);

    setError(error, errorSize, "User created successfully.");
    return 1;
}


/* =========================================
   VERIFY LOGIN
   ========================================= */

int verifyUser(sqlite3 *db,
               const char *username,
               const char *password,
               char *role,
               int roleSize)
{
    sqlite3_stmt *stmt = NULL;

    unsigned char salt[SALT_SIZE];
    unsigned char storedHash[HASH_SIZE];
    unsigned char enteredHash[HASH_SIZE];

    if (!username || !password || !role || roleSize <= 0)
        return 0;

    const char *sql =
        "SELECT password_hash, salt, role "
        "FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    const char *hashText =
        (const char *)sqlite3_column_text(stmt, 0);

    const char *saltText =
        (const char *)sqlite3_column_text(stmt, 1);

    const char *userRole =
        (const char *)sqlite3_column_text(stmt, 2);

    char hashCopy[HASH_SIZE * 2 + 1];
    char saltCopy[SALT_SIZE * 2 + 1];
    char roleCopy[16];

    snprintf(hashCopy, sizeof(hashCopy), "%s",
             hashText ? hashText : "");

    snprintf(saltCopy, sizeof(saltCopy), "%s",
             saltText ? saltText : "");

    snprintf(roleCopy, sizeof(roleCopy), "%s",
             userRole ? userRole : "");

    sqlite3_finalize(stmt);

    if (!hexToBytes(hashCopy, storedHash, HASH_SIZE) ||
        !hexToBytes(saltCopy, salt, SALT_SIZE))
        return 0;

    if (!hashPassword(password, salt, enteredHash))
        return 0;

    if (!secureCompare(storedHash, enteredHash, HASH_SIZE))
        return 0;

    snprintf(role, roleSize, "%s", roleCopy);

    return 1;
}


/* =========================================
   CHECK AND CHANGE FIRST-LOGIN PASSWORD
   ========================================= */

int userMustChangePassword(sqlite3 *db, const char *username)
{
    sqlite3_stmt *stmt = NULL;

    if (!username)
        return 0;

    const char *sql =
        "SELECT must_change_password "
        "FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    int result = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    return result;
}


int changeUserPassword(sqlite3 *db,
                       const char *username,
                       const char *oldPassword,
                       const char *newPassword,
                       char *error,
                       int errorSize)
{
    sqlite3_stmt *stmt = NULL;

    char role[16];

    unsigned char salt[SALT_SIZE];
    unsigned char hash[HASH_SIZE];

    char saltHex[SALT_SIZE * 2 + 1];
    char hashHex[HASH_SIZE * 2 + 1];

    if (!username || !oldPassword ||
        !newPassword || strlen(newPassword) < 8)
    {
        setError(error, errorSize,
                 "New password must have at least 8 characters.");
        return 0;
    }

    if (!verifyUser(db, username, oldPassword,
                    role, sizeof(role)))
    {
        setError(error, errorSize, "Current password is incorrect.");
        return 0;
    }

    if (!generateSalt(salt) ||
        !hashPassword(newPassword, salt, hash))
    {
        setError(error, errorSize, "Password hashing failed.");
        return 0;
    }

    bytesToHex(salt, SALT_SIZE, saltHex);
    bytesToHex(hash, HASH_SIZE, hashHex);

    const char *sql =
        "UPDATE users SET password_hash = ?, salt = ?, "
        "must_change_password = 0 WHERE username = ?;";

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, hashHex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, saltHex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        setError(error, errorSize, sqlite3_errmsg(db));
        return 0;
    }

    setError(error, errorSize, "Password changed successfully.");
    return 1;
}


/* =========================================
   CHECK WHETHER ANY USERS EXIST
   ========================================= */

int hasAnyUsers(sqlite3 *db)
{
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(
        db,
        "SELECT 1 FROM users LIMIT 1;",
        -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    int result = sqlite3_step(stmt) == SQLITE_ROW;

    sqlite3_finalize(stmt);

    return result;
}


/* =========================================
   ASSIGN MANAGER FLOOR
   ========================================= */

int assignManagerFloor(sqlite3 *db,
                       const char *username,
                       const char *building,
                       int floor)
{
    sqlite3_stmt *stmt = NULL;

    if (!username || !validBuilding(building) ||
        floor < 0 || floor > 100)
        return 0;

    if (sqlite3_prepare_v2(
        db,
        "INSERT OR IGNORE INTO manager_assignments "
        "(username, building, floor) "
        "SELECT username, ?, ? FROM users "
        "WHERE username = ? AND role = 'MANAGER';",
        -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, building, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, floor);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db);

    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE && changed > 0;
}


/* =========================================
   CHECK MANAGER ACCESS
   ========================================= */

int managerCanAccessRoom(sqlite3 *db,
                         const char *username,
                         const char *building,
                         int floor)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "SELECT 1 FROM manager_assignments "
        "WHERE username = ? AND building = ? AND floor = ?;";

    if (!username || !building)
        return 0;

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, building, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, floor);

    int result = sqlite3_step(stmt) == SQLITE_ROW;

    sqlite3_finalize(stmt);

    return result;
}
