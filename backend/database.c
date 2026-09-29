#include <stdio.h>
#include <string.h>
#include "sqlite3.h"

/*
    This callback function prints every available room
    returned by SQLite.
*/
int displayAvailableRoom(
    void *data,
    int columnCount,
    char **columnValues,
    char **columnNames
)
{
    int i;

    for (i = 0; i < columnCount; i++)
    {
        printf("%s: %s\n",
               columnNames[i],
               columnValues[i] ? columnValues[i] : "NULL");
    }

    printf("-------------------------\n");

    return 0;
}


int main()
{
    sqlite3 *db;

    char *errorMessage = NULL;

    int result;

    char day[20];
    char startTime[10];
    char endTime[10];

    char sql[1000];


    /* -----------------------------------------
       STEP 1: Open RoomSync database
       ----------------------------------------- */

    result = sqlite3_open("database/roomsync.db", &db);

    if (result != SQLITE_OK)
    {
        printf("Error opening database: %s\n",
               sqlite3_errmsg(db));

        sqlite3_close(db);

        return 1;
    }

    printf("Database connected successfully!\n\n");


    /* -----------------------------------------
       STEP 2: Get search details from user
       ----------------------------------------- */

    printf("Enter day: ");
    scanf("%19s", day);

    printf("Enter start time (HH:MM): ");
    scanf("%9s", startTime);

    printf("Enter end time (HH:MM): ");
    scanf("%9s", endTime);


    /* -----------------------------------------
       STEP 3: Create SQL query

       A room is available when:

       1. Floor Manager status = EMPTY

       AND

       2. There is NO routine that overlaps
          with the requested time.
       ----------------------------------------- */

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


    /* -----------------------------------------
       STEP 4: Execute search
       ----------------------------------------- */

    printf("\nAvailable Rooms\n");
    printf("=========================\n");

    result = sqlite3_exec(
        db,
        sql,
        displayAvailableRoom,
        NULL,
        &errorMessage
    );


    /* -----------------------------------------
       STEP 5: Check for SQL errors
       ----------------------------------------- */

    if (result != SQLITE_OK)
    {
        printf("SQL Error: %s\n",
               errorMessage);

        sqlite3_free(errorMessage);
    }


    /* -----------------------------------------
       STEP 6: Close database
       ----------------------------------------- */

    sqlite3_close(db);

    return 0;
}