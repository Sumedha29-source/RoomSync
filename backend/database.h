#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"

sqlite3 *openDatabase();

void closeDatabase(sqlite3 *db);

void findAvailableRooms(
    sqlite3 *db,
    const char *day,
    const char *startTime,
    const char *endTime
);

int updateRoomStatus(
    sqlite3 *db,
    const char *roomNumber,
    const char *status
);

int getAllRoomsJSON(
    sqlite3 *db,
    char *output,
    int outputSize
);

#endif