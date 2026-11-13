
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include "database.h"

#define PORT 8080
#define REQUEST_SIZE 16384
#define RESPONSE_SIZE 16384
#define JSON_SIZE 8192
#define MAX_SESSIONS 100

/* =========================================
   SESSION STORAGE
   ========================================= */

typedef struct
{
    char token[65];
    char username[41];
    char role[16];
    int active;
    int mustChangePassword;
} Session;

static Session sessions[MAX_SESSIONS];


/* =========================================
   BASIC HELPERS
   ========================================= */

static int startsWith(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static void setMessage(char *output, size_t size,
                       const char *message)
{
    snprintf(output, size, "%s", message);
}

static int parsePositiveInt(const char *value, int *result)
{
    char *end = NULL;
    long number;

    if (!value || !*value)
        return 0;

    number = strtol(value, &end, 10);

    if (*end != '\0' || number < 0 || number > 10000)
        return 0;

    *result = (int)number;
    return 1;
}

static int validUsername(const char *username)
{
    size_t i;

    if (!username || !*username || strlen(username) > 40)
        return 0;

    for (i = 0; username[i]; i++)
    {
        if (!isalnum((unsigned char)username[i]) &&
            username[i] != '_' &&
            username[i] != '-' &&
            username[i] != '.')
            return 0;
    }

    return 1;
}

static int validBuildingName(const char *building)
{
    return building &&
        (strcmp(building, "ICT") == 0 ||
         strcmp(building, "CME") == 0 ||
         strcmp(building, "CB") == 0);
}


/* =========================================
   URL DECODING
   ========================================= */

static void urlDecode(const char *source,
                      char *destination,
                      size_t destinationSize)
{
    size_t out = 0;

    while (*source && out + 1 < destinationSize)
    {
        if (*source == '+')
        {
            destination[out++] = ' ';
            source++;
        }
        else if (*source == '%' &&
                 isxdigit((unsigned char)source[1]) &&
                 isxdigit((unsigned char)source[2]))
        {
            char hex[3];

            hex[0] = source[1];
            hex[1] = source[2];
            hex[2] = '\0';

            destination[out++] =
                (char)strtol(hex, NULL, 16);

            source += 3;
        }
        else
        {
            destination[out++] = *source++;
        }
    }

    destination[out] = '\0';
}


/* =========================================
   QUERY STRING READER
   ========================================= */

static int getQueryValue(const char *requestLine,
                         const char *key,
                         char *output,
                         size_t outputSize)
{
    char search[80];

    const char *query;
    const char *found;
    const char *valueEnd;

    size_t keyLength;

    if (snprintf(search, sizeof(search),
                 "%s=", key) >= (int)sizeof(search))
        return 0;

    query = strchr(requestLine, '?');

    if (!query)
        return 0;

    query++;
    keyLength = strlen(search);
    found = query;

    while (*found)
    {
        if ((found == query || found[-1] == '&') &&
            strncmp(found, search, keyLength) == 0)
        {
            found += keyLength;

            valueEnd = strchr(found, '&');

            if (!valueEnd)
            {
                valueEnd = strchr(found, ' ');

                if (!valueEnd)
                    valueEnd = found + strlen(found);
            }

            {
                size_t length = (size_t)(valueEnd - found);
                char raw[512];

                if (length >= sizeof(raw))
                    return 0;

                memcpy(raw, found, length);
                raw[length] = '\0';

                urlDecode(raw, output, outputSize);
                return 1;
            }
        }

        found = strchr(found, '&');

        if (!found)
            break;

        found++;
    }

    return 0;
}


/* =========================================
   FORM BODY READER
   ========================================= */

static int getFormValue(const char *body,
                        const char *key,
                        char *output,
                        size_t outputSize)
{
    char search[80];

    const char *found;
    const char *end;

    size_t keyLength;

    if (!body || !key || !output)
        return 0;

    if (snprintf(search, sizeof(search),
                 "%s=", key) >= (int)sizeof(search))
        return 0;

    keyLength = strlen(search);
    found = body;

    while (*found)
    {
        if ((found == body || found[-1] == '&') &&
            strncmp(found, search, keyLength) == 0)
        {
            found += keyLength;

            end = strchr(found, '&');

            if (!end)
                end = found + strlen(found);

            {
                size_t length = (size_t)(end - found);
                char raw[1024];

                if (length >= sizeof(raw))
                    return 0;

                memcpy(raw, found, length);
                raw[length] = '\0';

                urlDecode(raw, output, outputSize);
                return 1;
            }
        }

        found = strchr(found, '&');

        if (!found)
            break;

        found++;
    }

    return 0;
}


/* =========================================
   HTTP RESPONSE
   ========================================= */

static void sendResponse(SOCKET clientSocket,
                         const char *request,
                         int statusCode,
                         const char *statusText,
                         const char *contentType,
                         const char *message,
                         const char *extraHeader)
{
    char response[RESPONSE_SIZE];
    char origin[256] = "";

    const char *headerEnd;
    int bodyLength;
    int headerLength;
    int totalLength;
    int totalSent = 0;

    bodyLength = (int)strlen(message);

    headerEnd = strstr(request, "\r\n");

    if (headerEnd)
    {
        const char *originHeader = strstr(request, "\r\nOrigin: ");

        if (originHeader)
        {
            originHeader += 10;

            const char *end = strstr(originHeader, "\r\n");

            if (end)
            {
                size_t length = (size_t)(end - originHeader);

                if (length < sizeof(origin))
                {
                    memcpy(origin, originHeader, length);
                    origin[length] = '\0';
                }
            }
        }
    }

    headerLength = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "%s"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        statusCode,
        statusText,
        contentType,
        (strcmp(origin, "http://127.0.0.1:5500") == 0 ||
         strcmp(origin, "http://localhost:5500") == 0)
            ? (strcmp(origin, "http://localhost:5500") == 0
                ? "Access-Control-Allow-Origin: http://localhost:5500\r\n"
                : "Access-Control-Allow-Origin: http://127.0.0.1:5500\r\n")
            : "",
        bodyLength,
        extraHeader ? extraHeader : "");

    if (headerLength < 0 ||
        headerLength + bodyLength >= (int)sizeof(response))
    {
        printf("Response too large or invalid.\n");
        return;
    }

    memcpy(response + headerLength, message, bodyLength);

    totalLength = headerLength + bodyLength;

    while (totalSent < totalLength)
    {
        int sent = send(clientSocket,
                        response + totalSent,
                        totalLength - totalSent,
                        0);

        if (sent == SOCKET_ERROR || sent == 0)
        {
            printf("Send failed: %d\n", WSAGetLastError());
            return;
        }

        totalSent += sent;
    }
}

static void sendText(SOCKET clientSocket,
                     const char *request,
                     int statusCode,
                     const char *statusText,
                     const char *message)
{
    sendResponse(clientSocket, request,
                 statusCode, statusText,
                 "text/plain", message, NULL);
}

static void sendJSON(SOCKET clientSocket,
                     const char *request,
                     int statusCode,
                     const char *statusText,
                     const char *message)
{
    sendResponse(clientSocket, request,
                 statusCode, statusText,
                 "application/json", message, NULL);
}


/* =========================================
   READ HTTP REQUEST
   ========================================= */

static int readRequest(SOCKET clientSocket,
                       char *request,
                       size_t requestSize)
{
    int total = 0;
    int contentLength = 0;
    int headerLength = 0;

    char *headerEnd;

    while (total < (int)requestSize - 1)
    {
        int received = recv(
            clientSocket,
            request + total,
            (int)requestSize - 1 - total,
            0);

        if (received <= 0)
            return 0;

        total += received;
        request[total] = '\0';

        headerEnd = strstr(request, "\r\n\r\n");

        if (headerEnd)
        {
            char *lengthHeader;

            headerLength =
                (int)(headerEnd + 4 - request);

            lengthHeader = strstr(request, "Content-Length:");

            if (lengthHeader)
                contentLength = atoi(lengthHeader + 15);

            if (contentLength < 0 ||
                contentLength > (int)requestSize - headerLength - 1)
                return 0;

            if (total >= headerLength + contentLength)
                return 1;
        }
    }

    return 0;
}


/* =========================================
   SESSION HELPERS
   ========================================= */

static int createSession(const char *username,
                         const char *role,
                         int mustChangePassword,
                         char *token)
{
    unsigned char randomBytes[32];

    int i;
    int slot = -1;

    if (BCryptGenRandom(
        NULL,
        randomBytes,
        sizeof(randomBytes),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return 0;

    for (i = 0; i < MAX_SESSIONS; i++)
    {
        if (!sessions[i].active)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return 0;

    for (i = 0; i < 32; i++)
    {
        snprintf(token + (i * 2), 3,
                 "%02x", randomBytes[i]);
    }

    token[64] = '\0';

    snprintf(sessions[slot].token,
             sizeof(sessions[slot].token),
             "%s", token);

    snprintf(sessions[slot].username,
             sizeof(sessions[slot].username),
             "%s", username);

    snprintf(sessions[slot].role,
             sizeof(sessions[slot].role),
             "%s", role);

    sessions[slot].mustChangePassword =
        mustChangePassword;

    sessions[slot].active = 1;

    return 1;
}

static Session *findSession(const char *request)
{
    char cookieHeader[512] = "";
    char token[80] = "";

    const char *cookie;
    const char *end;
    int i;

    cookie = strstr(request, "\r\nCookie:");

    if (!cookie)
        return NULL;

    cookie += 10;

    while (*cookie == ' ')
        cookie++;

    end = strstr(cookie, "\r\n");

    if (!end)
        return NULL;

    {
        size_t length = (size_t)(end - cookie);

        if (length >= sizeof(cookieHeader))
            return NULL;

        memcpy(cookieHeader, cookie, length);
        cookieHeader[length] = '\0';
    }

    const char *found = strstr(cookieHeader,
                               "ROOMSYNC_SESSION=");

    if (!found)
        return NULL;

    found += strlen("ROOMSYNC_SESSION=");

    end = strchr(found, ';');

    if (!end)
        end = found + strlen(found);

    {
        size_t length = (size_t)(end - found);

        if (length != 64)
            return NULL;

        memcpy(token, found, length);
        token[length] = '\0';
    }

    for (i = 0; i < MAX_SESSIONS; i++)
    {
        if (sessions[i].active &&
            strcmp(sessions[i].token, token) == 0)
            return &sessions[i];
    }

    return NULL;
}

static void removeSession(Session *session)
{
    if (session)
        memset(session, 0, sizeof(Session));
}

static int isAdmin(Session *session)
{
    return session &&
           strcmp(session->role, "ADMIN") == 0;
}

static int canManage(Session *session)
{
    return session &&
           (strcmp(session->role, "ADMIN") == 0 ||
            strcmp(session->role, "MANAGER") == 0) &&
           !session->mustChangePassword;
}


/* =========================================
   ROOM LOCATION CHECK
   ========================================= */

static int getRoomLocation(sqlite3 *db,
                           const char *roomName,
                           char *building,
                           int buildingSize,
                           int *floor)
{
    sqlite3_stmt *stmt = NULL;

    const char *sql =
        "SELECT building, floor FROM rooms "
        "WHERE room_number = ?;";

    if (sqlite3_prepare_v2(db, sql, -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, roomName,
                      -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    const char *buildingText =
        (const char *)sqlite3_column_text(stmt, 0);

    if (!buildingText)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    snprintf(building, buildingSize, "%s", buildingText);
    *floor = sqlite3_column_int(stmt, 1);

    sqlite3_finalize(stmt);

    return 1;
}

static int managerCanManageRoom(sqlite3 *db,
                                Session *session,
                                const char *roomName)
{
    char building[16];
    int floor;

    if (!session)
        return 0;

    if (isAdmin(session))
        return 1;

    if (strcmp(session->role, "MANAGER") != 0)
        return 0;

    if (!getRoomLocation(db, roomName,
                         building, sizeof(building), &floor))
        return 0;

    return managerCanAccessRoom(
        db, session->username, building, floor);
}


/* =========================================
   MAIN SERVER
   ========================================= */

int main(void)
{
    WSADATA wsa;

    SOCKET serverSocket;
    SOCKET clientSocket;

    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    sqlite3 *db = NULL;

    memset(sessions, 0, sizeof(sessions));

    db = openDatabase();

    if (!db)
    {
        printf("Could not open RoomSync database.\n");
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Winsock initialization failed.\n");
        closeDatabase(db);
        return 1;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == INVALID_SOCKET)
    {
        printf("Could not create socket.\n");
        closeDatabase(db);
        WSACleanup();
        return 1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(PORT);

    if (bind(serverSocket,
             (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) == SOCKET_ERROR)
    {
        printf("Could not bind to port %d. "
               "Is another RoomSync server running?\n", PORT);

        closesocket(serverSocket);
        closeDatabase(db);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR)
    {
        printf("Listen failed.\n");
        closesocket(serverSocket);
        closeDatabase(db);
        WSACleanup();
        return 1;
    }

    printf("\nRoomSync C Server started!\n");
    printf("Listening on http://127.0.0.1:%d\n", PORT);
    printf("Press Ctrl+C to stop.\n");

    while (1)
    {
        char request[REQUEST_SIZE];
        char requestLine[512];
        char body[REQUEST_SIZE];

        char method[16] = "";
        char path[256] = "";

        char message[JSON_SIZE];

        char username[64];
        char password[128];
        char role[16];

        char room[64];
        char oldRoom[64];
        char building[16];
        char occupancyText[20];

        char floorText[16];
        char capacityText[16];

        char day[32];
        char start[16];
        char end[16];

        char error[256];

        int floor;
        int capacity;
        int occupancy;

        int clientAddressSize = sizeof(clientAddress);

        Session *session = NULL;

        clientSocket = accept(
            serverSocket,
            (struct sockaddr *)&clientAddress,
            &clientAddressSize);

        if (clientSocket == INVALID_SOCKET)
        {
            printf("Client connection failed.\n");
            continue;
        }

        memset(request, 0, sizeof(request));
        memset(body, 0, sizeof(body));

        if (!readRequest(clientSocket, request,
                         sizeof(request)))
        {
            sendText(clientSocket, request, 400,
                     "Bad Request", "Invalid or incomplete request.");

            closesocket(clientSocket);
            continue;
        }

        sscanf(request, "%15s %255s",
               method, path);

        {
            char *bodyStart = strstr(request, "\r\n\r\n");

            if (bodyStart)
                snprintf(body, sizeof(body), "%s", bodyStart + 4);
        }

        snprintf(requestLine, sizeof(requestLine),
                 "%s %s", method, path);

        printf("\nRequest: %s\n", requestLine);

        session = findSession(request);

        /* =====================================
           CORS PREFLIGHT
           ===================================== */

        if (strcmp(method, "OPTIONS") == 0)
        {
            sendText(clientSocket, request,
                     200, "OK", "OK");
        }

        /* =====================================
           SERVER TEST
           ===================================== */

        else if (strcmp(method, "GET") == 0 &&
                 strcmp(path, "/test") == 0)
        {
            sendText(clientSocket, request,
                     200, "OK",
                     "RoomSync C Server is working!");
        }

        /* =====================================
           INITIAL ADMIN SETUP
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/setup-admin") == 0)
        {
            char adminUsername[64];
            char adminPassword[128];

            if (hasAnyUsers(db))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "Admin setup has already been completed.");
            }
            else if (!getFormValue(body, "username",
                                   adminUsername,
                                   sizeof(adminUsername)) ||
                     !getFormValue(body, "password",
                                   adminPassword,
                                   sizeof(adminPassword)) ||
                     !validUsername(adminUsername))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Provide a valid username and password.");
            }
            else if (createUser(db,
                                adminUsername,
                                adminPassword,
                                "ADMIN",
                                0,
                                error,
                                sizeof(error)))
            {
                sendText(clientSocket, request,
                         200, "OK",
                         "Admin account created. You can now log in.");
            }
            else
            {
                sendText(clientSocket, request,
                         400, "Bad Request", error);
            }
        }

        /* =====================================
           LOGIN
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/login") == 0)
        {
            char token[65];

            if (!getFormValue(body, "username",
                              username, sizeof(username)) ||
                !getFormValue(body, "password",
                              password, sizeof(password)))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Username and password are required.");
            }
            else if (!verifyUser(db, username, password,
                                 role, sizeof(role)))
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Invalid username or password.");
            }
            else
            {
                int mustChange =
                    userMustChangePassword(db, username);

                if (!createSession(username, role,
                                   mustChange, token))
                {
                    sendText(clientSocket, request,
                             500, "Internal Server Error",
                             "Could not create login session.");
                }
                else
                {
                    snprintf(message, sizeof(message),
                        "{\"success\":true,"
                        "\"username\":\"%s\","
                        "\"role\":\"%s\","
                        "\"mustChangePassword\":%s}",
                        username,
                        role,
                        mustChange ? "true" : "false");

                    char cookie[256];

                    snprintf(cookie, sizeof(cookie),
                        "Set-Cookie: ROOMSYNC_SESSION=%s; "
                        "Path=/; HttpOnly; SameSite=Lax; Max-Age=28800\r\n",
                        token);

                    sendResponse(clientSocket, request,
                                 200, "OK",
                                 "application/json",
                                 message, cookie);
                }
            }
        }

        /* =====================================
           CURRENT SESSION
           ===================================== */

        else if (strcmp(method, "GET") == 0 &&
                 strcmp(path, "/session") == 0)
        {
            if (!session)
            {
                sendJSON(clientSocket, request,
                         200, "OK",
                         "{\"authenticated\":false}");
            }
            else
            {
                snprintf(message, sizeof(message),
                    "{\"authenticated\":true,"
                    "\"username\":\"%s\","
                    "\"role\":\"%s\","
                    "\"mustChangePassword\":%s}",
                    session->username,
                    session->role,
                    session->mustChangePassword
                        ? "true" : "false");

                sendJSON(clientSocket, request,
                         200, "OK", message);
            }
        }

        /* =====================================
           LOGOUT
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/logout") == 0)
        {
            removeSession(session);

            sendResponse(clientSocket, request,
                         200, "OK",
                         "text/plain",
                         "Logged out successfully.",
                         "Set-Cookie: ROOMSYNC_SESSION=; "
                         "Path=/; HttpOnly; SameSite=Lax; Max-Age=0\r\n");
        }

        /* =====================================
           CHANGE PASSWORD
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/change-password") == 0)
        {
            char oldPassword[128];
            char newPassword[128];

            if (!session)
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Please log in first.");
            }
            else if (!getFormValue(body, "oldPassword",
                                   oldPassword,
                                   sizeof(oldPassword)) ||
                     !getFormValue(body, "newPassword",
                                   newPassword,
                                   sizeof(newPassword)))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Current and new passwords are required.");
            }
            else if (changeUserPassword(
                         db,
                         session->username,
                         oldPassword,
                         newPassword,
                         error,
                         sizeof(error)))
            {
                session->mustChangePassword = 0;

                sendText(clientSocket, request,
                         200, "OK", error);
            }
            else
            {
                sendText(clientSocket, request,
                         400, "Bad Request", error);
            }
        }

        
        /* =====================================
           MANAGER ASSIGNED ROOM LIST
           ===================================== */

        else if (strcmp(method, "GET") == 0 &&
                 strcmp(path, "/manager-rooms") == 0)
        {
            if (!session ||
                strcmp(session->role, "MANAGER") != 0)
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Floor Manager login required.");
            }
            else if (getManagerRoomsJSON(
                         db,
                         session->username,
                         message,
                         sizeof(message)))
            {
                sendJSON(clientSocket, request,
                         200, "OK", message);
            }
            else
            {
                sendJSON(clientSocket, request,
                         500, "Internal Server Error", "[]");
            }
        }


        /* =====================================
           PUBLIC ROOM LIST
           ===================================== */

        else if (strcmp(method, "GET") == 0 &&
                 strcmp(path, "/rooms") == 0)
        {
            if (getAllRoomsJSON(db, message,
                                sizeof(message)))
            {
                sendJSON(clientSocket, request,
                         200, "OK", message);
            }
            else
            {
                sendJSON(clientSocket, request,
                         500, "Internal Server Error", "[]");
            }
        }

        /* =====================================
           PUBLIC ROOM AVAILABILITY
           ===================================== */

        else if (strcmp(method, "GET") == 0 &&
                 startsWith(path, "/available-rooms?"))
        {
            if (!getQueryValue(requestLine, "day",
                               day, sizeof(day)) ||
                !getQueryValue(requestLine, "start",
                               start, sizeof(start)) ||
                !getQueryValue(requestLine, "end",
                               end, sizeof(end)))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Invalid room search request.");
            }
            else if (strcmp(start, end) >= 0)
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "End time must be later than start time.");
            }
            else if (getAvailableRoomsJSON(
                         db, day, start, end,
                         message, sizeof(message)))
            {
                sendJSON(clientSocket, request,
                         200, "OK", message);
            }
            else
            {
                sendJSON(clientSocket, request,
                         500, "Internal Server Error", "[]");
            }
        }

        /* =====================================
           CREATE FLOOR MANAGER
           Admin only
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/manager-create") == 0)
        {
            char managerUsername[64];
            char managerPassword[128];

            char floorsText[256];
            char floorsCopy[256];

            int floorList[50];
            int floorCount = 0;

            char *part;

            if (!session || !isAdmin(session))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "Only Admin can create Floor Managers.");
            }
            else if (!canManage(session))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "Change your password before continuing.");
            }
            else if (!getFormValue(body, "username",
                                   managerUsername,
                                   sizeof(managerUsername)) ||
                     !getFormValue(body, "password",
                                   managerPassword,
                                   sizeof(managerPassword)) ||
                     !getFormValue(body, "building",
                                   building,
                                   sizeof(building)) ||
                     !getFormValue(body, "floors",
                                   floorsText,
                                   sizeof(floorsText)) ||
                     !validUsername(managerUsername) ||
                     !validBuildingName(building))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Provide a valid username, password, building and floors.");
            }
            else
            {
                snprintf(floorsCopy, sizeof(floorsCopy),
                         "%s", floorsText);

                part = strtok(floorsCopy, ",");

                while (part && floorCount < 50)
                {
                    int value;

                    while (*part == ' ')
                        part++;

                    if (!parsePositiveInt(part, &value) ||
                        value > 100)
                    {
                        floorCount = 0;
                        break;
                    }

                    floorList[floorCount++] = value;
                    part = strtok(NULL, ",");
                }

                if (floorCount == 0)
                {
                    sendText(clientSocket, request,
                             400, "Bad Request",
                             "Enter at least one valid floor.");
                }
                else if (!executeSQL(db, "BEGIN TRANSACTION;"))
                {
                    sendText(clientSocket, request,
                             500, "Internal Server Error",
                             "Could not start account creation.");
                }
                else if (!createUser(db,
                                     managerUsername,
                                     managerPassword,
                                     "MANAGER",
                                     1,
                                     error,
                                     sizeof(error)))
                {
                    executeSQL(db, "ROLLBACK;");

                    sendText(clientSocket, request,
                             400, "Bad Request", error);
                }
                else
                {
                    int success = 1;
                    int i;

                    for (i = 0; i < floorCount; i++)
                    {
                        if (!assignManagerFloor(
                                db,
                                managerUsername,
                                building,
                                floorList[i]))
                        {
                            success = 0;
                            break;
                        }
                    }

                    if (success &&
                        executeSQL(db, "COMMIT;"))
                    {
                        sendText(clientSocket, request,
                                 200, "OK",
                                 "Floor Manager account created successfully. They must change their temporary password at first login.");
                    }
                    else
                    {
                        executeSQL(db, "ROLLBACK;");

                        sendText(clientSocket, request,
                                 500, "Internal Server Error",
                                 "Could not complete manager assignments.");
                    }
                }
            }
        }

        /* =====================================
           UPDATE ROOM OCCUPANCY
           Admin or assigned Manager
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/room-status") == 0)
        {
            if (!canManage(session))
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Login with a valid management account first.");
            }
            else if (!getFormValue(body, "room",
                                   room, sizeof(room)) ||
                     !getFormValue(body, "status",
                                   role, sizeof(role)) ||
                     !getFormValue(body, "occupancy",
                                   occupancyText, sizeof(occupancyText)))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Room, status and occupancy are required.");
            }
            else if (!parsePositiveInt(occupancyText, &occupancy))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Occupancy must be a valid non-negative number.");
            }
            else if (!managerCanManageRoom(db, session, room))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "You do not have permission to update this room.");
            }
            else if (updateRoomStatus(db, room, role, occupancy))
            {
                sendText(clientSocket, request,
                         200, "OK",
                         "Room status and occupancy updated successfully.");
            }
            else
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Update failed. Check status, occupancy and room capacity.");
            }
        }

        /* =====================================
           ADD ROOM
           Admin or assigned Manager
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/room-add") == 0)
        {
            if (!canManage(session))
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Login with a valid management account first.");
            }
            else if (!getFormValue(body, "room",
                                   room, sizeof(room)) ||
                     !getFormValue(body, "building",
                                   building, sizeof(building)) ||
                     !getFormValue(body, "floor",
                                   floorText, sizeof(floorText)) ||
                     !getFormValue(body, "capacity",
                                   capacityText, sizeof(capacityText)) ||
                     !parsePositiveInt(floorText, &floor) ||
                     !parsePositiveInt(capacityText, &capacity))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Provide a room name, building, floor and capacity.");
            }
            else if (!validBuildingName(building))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Building must be ICT, CME or CB.");
            }
            else if (!isAdmin(session) &&
                     !managerCanAccessRoom(
                         db, session->username,
                         building, floor))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "You cannot add rooms outside your assigned floors.");
            }
            else if (addRoom(db, room, building,
                             floor, capacity,
                             error, sizeof(error)))
            {
                sendText(clientSocket, request,
                         200, "OK", error);
            }
            else
            {
                sendText(clientSocket, request,
                         400, "Bad Request", error);
            }
        }

        /* =====================================
           EDIT ROOM
           Admin or assigned Manager
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/room-edit") == 0)
        {
            if (!canManage(session))
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Login with a valid management account first.");
            }
            else if (!getFormValue(body, "old",
                                   oldRoom, sizeof(oldRoom)) ||
                     !getFormValue(body, "room",
                                   room, sizeof(room)) ||
                     !getFormValue(body, "building",
                                   building, sizeof(building)) ||
                     !getFormValue(body, "floor",
                                   floorText, sizeof(floorText)) ||
                     !getFormValue(body, "capacity",
                                   capacityText, sizeof(capacityText)) ||
                     !parsePositiveInt(floorText, &floor) ||
                     !parsePositiveInt(capacityText, &capacity))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Provide the old room, new room, building, floor and capacity.");
            }
            else if (!validBuildingName(building))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Building must be ICT, CME or CB.");
            }
            else if (!isAdmin(session) &&
                     (!managerCanManageRoom(db, session, oldRoom) ||
                      !managerCanAccessRoom(
                          db, session->username,
                          building, floor)))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "You cannot edit or move rooms outside your assigned floors.");
            }
            else if (editRoom(db, oldRoom, room,
                              building, floor, capacity,
                              error, sizeof(error)))
            {
                sendText(clientSocket, request,
                         200, "OK", error);
            }
            else
            {
                sendText(clientSocket, request,
                         400, "Bad Request", error);
            }
        }

        /* =====================================
           DELETE ROOM
           Admin or assigned Manager
           ===================================== */

        else if (strcmp(method, "POST") == 0 &&
                 strcmp(path, "/room-delete") == 0)
        {
            if (!canManage(session))
            {
                sendText(clientSocket, request,
                         401, "Unauthorized",
                         "Login with a valid management account first.");
            }
            else if (!getFormValue(body, "room",
                                   room, sizeof(room)))
            {
                sendText(clientSocket, request,
                         400, "Bad Request",
                         "Room name is required.");
            }
            else if (!managerCanManageRoom(db, session, room))
            {
                sendText(clientSocket, request,
                         403, "Forbidden",
                         "You cannot delete this room.");
            }
            else if (deleteRoom(db, room,
                                error, sizeof(error)))
            {
                sendText(clientSocket, request,
                         200, "OK", error);
            }
            else
            {
                sendText(clientSocket, request,
                         400, "Bad Request", error);
            }
        }

        /* =====================================
           UNKNOWN ROUTE
           ===================================== */

        else
        {
            sendText(clientSocket, request,
                     404, "Not Found",
                     "RoomSync server: route not found.");
        }

        closesocket(clientSocket);
    }

    closesocket(serverSocket);
    closeDatabase(db);
    WSACleanup();

    return 0;
}
