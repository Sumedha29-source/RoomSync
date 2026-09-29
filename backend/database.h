#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"

/* Open database */
sqlite3 *openDatabase();

/* Close database */
void closeDatabase(sqlite3 *db);


/* Terminal version - kept for testing */
void findAvailableRooms(
    sqlite3 *db,
    const char *day,
    const char *startTime,
    const char *endTime
);


/* Floor Manager updates physical room status */
int updateRoomStatus(
    sqlite3 *db,
    const char *roomNumber,
    const char *status
);


/* Return all rooms as JSON */
int getAllRoomsJSON(
    sqlite3 *db,
    char *output,
    int outputSize
);


/* Return genuinely available rooms as JSON */
int getAvailableRoomsJSON(
    sqlite3 *db,
    const char *day,
    const char *startTime,
    const char *endTime,
    char *output,
    int outputSize
);

#endif