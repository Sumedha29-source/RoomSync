
#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"

/* Database connection */
sqlite3 *openDatabase(void);
void closeDatabase(sqlite3 *db);

int executeSQL(sqlite3 *db, const char *sql);

/* Room operations */
int getAllRoomsJSON(sqlite3 *db, char *output, int outputSize);

int getManagerRoomsJSON(
    sqlite3 *db,
    const char *username,
    char *output,
    int outputSize
);

int getAvailableRoomsJSON(
    sqlite3 *db,
    const char *day,
    const char *startTime,
    const char *endTime,
    char *output,
    int outputSize
);

int updateRoomStatus(
    sqlite3 *db,
    const char *roomName,
    const char *status
);

/* Room management */
int addRoom(
    sqlite3 *db,
    const char *roomName,
    const char *building,
    int floor,
    int capacity,
    char *error,
    int errorSize
);

int editRoom(
    sqlite3 *db,
    const char *oldRoomName,
    const char *newRoomName,
    const char *building,
    int floor,
    int capacity,
    char *error,
    int errorSize
);

int deleteRoom(
    sqlite3 *db,
    const char *roomName,
    char *error,
    int errorSize
);

/* Authentication */
int createUser(
    sqlite3 *db,
    const char *username,
    const char *password,
    const char *role,
    int mustChangePassword,
    char *error,
    int errorSize
);

int verifyUser(
    sqlite3 *db,
    const char *username,
    const char *password,
    char *role,
    int roleSize
);

int changeUserPassword(
    sqlite3 *db,
    const char *username,
    const char *oldPassword,
    const char *newPassword,
    char *error,
    int errorSize
);

int userMustChangePassword(sqlite3 *db, const char *username);

int hasAnyUsers(sqlite3 *db);

/* Manager assignments */
int assignManagerFloor(
    sqlite3 *db,
    const char *username,
    const char *building,
    int floor
);

int managerCanAccessRoom(
    sqlite3 *db,
    const char *username,
    const char *building,
    int floor
);

#endif
