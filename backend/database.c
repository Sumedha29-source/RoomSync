#include <stdio.h>
#include <string.h>
#include "database.h"

/*
    Callback function used by SQLite.
    It prints each available room returned by the query.
*/
static int displayAvailableRoom(
    void *data,
    int columnCount,
    char **columnValues,
    char **columnNames
)
{
    int i;

    for (i = 0; i < columnCount; i++)
    {
        printf(
            "%s: %s\n",
            columnNames[i],
            columnValues[i] ? columnValues[i] : "NULL"
        );
    }

    printf("-------------------------\n");

    return 0;
}


/*
    Opens the RoomSync database.
*/
sqlite3 *openDatabase()
{
    sqlite3 *db;
    int result;

    result = sqlite3_open(
        "database/roomsync.db",
        &db
    );

    if (result != SQLITE_OK)
    {
        printf(
            "Error opening database: %s\n",
            sqlite3_errmsg(db)
        );

        sqlite3_close(db);

        return NULL;
    }

    return db;
}


/*
    Closes the RoomSync database.
*/
void closeDatabase(sqlite3 *db)
{
    if (db != NULL)
    {
        sqlite3_close(db);
    }
}


/*
    Finds rooms that:
    1. Have no timetable conflict
    2. Are marked EMPTY by the Floor Manager
*/
void findAvailableRooms(
    sqlite3 *db,
    const char *day,
    const char *startTime,
    const char *endTime
)
{
    char sql[1000];
    char *errorMessage = NULL;

    int result;

    snprintf(
        sql,
        sizeof(sql),

        "SELECT room_number, floor, capacity "
        "FROM rooms "
        "WHERE status = 'EMPTY' "
        "AND room_id NOT IN ("
            "SELECT room_id "
            "FROM routines "
            "WHERE day = '%s' "
            "AND start_time < '%s' "
            "AND end_time > '%s'"
        ");",

        day,
        endTime,
        startTime
    );

    printf("\nAvailable Rooms\n");
    printf("=========================\n");

    result = sqlite3_exec(
        db,
        sql,
        displayAvailableRoom,
        NULL,
        &errorMessage
    );

    if (result != SQLITE_OK)
    {
        printf(
            "SQL Error: %s\n",
            errorMessage
        );

        sqlite3_free(errorMessage);
    }
}

int updateRoomStatus(
    sqlite3 *db,
    const char *roomNumber,
    const char *status
)
{
    sqlite3_stmt *statement;

    const char *sql =
        "UPDATE rooms "
        "SET status = ?, "
        "status_updated_at = datetime('now') "
        "WHERE room_number = ?;";

    int result;


    /* Only allow valid statuses */
    if (
        strcmp(status, "EMPTY") != 0 &&
        strcmp(status, "OCCUPIED") != 0
    )
    {
        printf("Invalid room status.\n");
        return 0;
    }


    /* Prepare SQL statement */
    result = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &statement,
        NULL
    );

    if (result != SQLITE_OK)
    {
        printf(
            "SQL preparation error: %s\n",
            sqlite3_errmsg(db)
        );

        return 0;
    }


    /* Replace first ? with status */
    sqlite3_bind_text(
        statement,
        1,
        status,
        -1,
        SQLITE_TRANSIENT
    );


    /* Replace second ? with room number */
    sqlite3_bind_text(
        statement,
        2,
        roomNumber,
        -1,
        SQLITE_TRANSIENT
    );


    /* Execute UPDATE */
    result = sqlite3_step(statement);

    if (result != SQLITE_DONE)
    {
        printf(
            "Room status update failed: %s\n",
            sqlite3_errmsg(db)
        );

        sqlite3_finalize(statement);

        return 0;
    }


    /*
        Check whether a room was actually found.
    */
    if (sqlite3_changes(db) == 0)
    {
        printf("Room %s was not found.\n", roomNumber);

        sqlite3_finalize(statement);

        return 0;
    }


    printf(
        "Room %s status updated to %s.\n",
        roomNumber,
        status
    );


    sqlite3_finalize(statement);

    return 1;
}

int getAllRoomsJSON(
    sqlite3 *db,
    char *output,
    int outputSize
)
{
    sqlite3_stmt *statement;

    const char *sql =
        "SELECT room_number, floor, capacity, status "
        "FROM rooms "
        "ORDER BY floor, room_number;";

    int result;
    int firstRoom = 1;

    int used = 0;

    result = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &statement,
        NULL
    );

    if (result != SQLITE_OK)
    {
        printf(
            "Could not read rooms: %s\n",
            sqlite3_errmsg(db)
        );

        return 0;
    }


    used += snprintf(
        output + used,
        outputSize - used,
        "["
    );


    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        const unsigned char *roomNumber =
            sqlite3_column_text(statement, 0);

        int floor =
            sqlite3_column_int(statement, 1);

        int capacity =
            sqlite3_column_int(statement, 2);

        const unsigned char *status =
            sqlite3_column_text(statement, 3);


        if (!firstRoom)
        {
            used += snprintf(
                output + used,
                outputSize - used,
                ","
            );
        }


        used += snprintf(
            output + used,
            outputSize - used,

            "{\"room_number\":\"%s\","
            "\"floor\":%d,"
            "\"capacity\":%d,"
            "\"status\":\"%s\"}",

            roomNumber,
            floor,
            capacity,
            status
        );


        firstRoom = 0;


        if (used >= outputSize - 100)
        {
            break;
        }
    }


    snprintf(
        output + used,
        outputSize - used,
        "]"
    );


    sqlite3_finalize(statement);

    return 1;
}

int getAvailableRoomsJSON(
    sqlite3 *db,
    const char *day,
    const char *startTime,
    const char *endTime,
    char *output,
    int outputSize
)
{
    sqlite3_stmt *statement;

    const char *sql =
        "SELECT room_number, floor, capacity "
        "FROM rooms "
        "WHERE status = 'EMPTY' "
        "AND room_id NOT IN ("
            "SELECT room_id "
            "FROM routines "
            "WHERE day = ? "
            "AND start_time < ? "
            "AND end_time > ?"
        ") "
        "ORDER BY floor, room_number;";

    int result;
    int firstRoom = 1;
    int used = 0;


    /* Prepare SQL query */

    result = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &statement,
        NULL
    );


    if (result != SQLITE_OK)
    {
        printf(
            "Could not search rooms: %s\n",
            sqlite3_errmsg(db)
        );

        return 0;
    }


    /*
        Replace the ? values:

        day = ?
        start_time < requested END
        end_time > requested START
    */

    sqlite3_bind_text(
        statement,
        1,
        day,
        -1,
        SQLITE_TRANSIENT
    );


    sqlite3_bind_text(
        statement,
        2,
        endTime,
        -1,
        SQLITE_TRANSIENT
    );


    sqlite3_bind_text(
        statement,
        3,
        startTime,
        -1,
        SQLITE_TRANSIENT
    );


    /* Start JSON array */

    used += snprintf(
        output + used,
        outputSize - used,
        "["
    );


    /* Read available rooms */

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        const unsigned char *roomNumber =
            sqlite3_column_text(statement, 0);

        int floor =
            sqlite3_column_int(statement, 1);

        int capacity =
            sqlite3_column_int(statement, 2);


        if (!firstRoom)
        {
            used += snprintf(
                output + used,
                outputSize - used,
                ","
            );
        }


        used += snprintf(
            output + used,
            outputSize - used,

            "{\"room_number\":\"%s\","
            "\"floor\":%d,"
            "\"capacity\":%d}",

            roomNumber,
            floor,
            capacity
        );


        firstRoom = 0;


        if (used >= outputSize - 100)
        {
            break;
        }
    }


    /* Close JSON array */

    snprintf(
        output + used,
        outputSize - used,
        "]"
    );


    sqlite3_finalize(statement);

    return 1;
}