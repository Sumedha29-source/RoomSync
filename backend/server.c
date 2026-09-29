#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include "database.h"

#define PORT 8080


/*
    Sends a normal HTTP response back to the browser.
*/
void sendResponse(SOCKET clientSocket, const char *message)
{
    char response[8192];

    snprintf(
        response,
        sizeof(response),

        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",

        message
    );

    send(
        clientSocket,
        response,
        strlen(response),
        0
    );
}


/*
    Main C HTTP server
*/
int main()
{
    WSADATA wsa;

    SOCKET serverSocket;
    SOCKET clientSocket;

    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    int clientAddressSize;

    char request[4096];

    sqlite3 *db;


    /* =========================================
       STEP 1
       Open RoomSync SQLite database
       ========================================= */

    db = openDatabase();

    if (db == NULL)
    {
        printf("Could not open RoomSync database.\n");

        return 1;
    }


    /* =========================================
       STEP 2
       Initialize Windows sockets
       ========================================= */

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Winsock initialization failed.\n");

        closeDatabase(db);

        return 1;
    }


    /* =========================================
       STEP 3
       Create server socket
       ========================================= */

    serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );


    if (serverSocket == INVALID_SOCKET)
    {
        printf("Could not create socket.\n");

        closeDatabase(db);

        WSACleanup();

        return 1;
    }


    /* =========================================
       STEP 4
       Configure server address
       ========================================= */

    serverAddress.sin_family = AF_INET;

    serverAddress.sin_addr.s_addr =
        inet_addr("127.0.0.1");

    serverAddress.sin_port =
        htons(PORT);


    /* =========================================
       STEP 5
       Bind server to port 8080
       ========================================= */

    if (
        bind(
            serverSocket,
            (struct sockaddr *)&serverAddress,
            sizeof(serverAddress)
        ) == SOCKET_ERROR
    )
    {
        printf(
            "Could not bind to port %d.\n",
            PORT
        );

        closesocket(serverSocket);

        closeDatabase(db);

        WSACleanup();

        return 1;
    }


    /* =========================================
       STEP 6
       Start listening
       ========================================= */

    if (
        listen(
            serverSocket,
            5
        ) == SOCKET_ERROR
    )
    {
        printf("Listen failed.\n");

        closesocket(serverSocket);

        closeDatabase(db);

        WSACleanup();

        return 1;
    }


    printf("\nRoomSync C Server started!\n");

    printf(
        "Listening on http://localhost:%d\n",
        PORT
    );

    printf(
        "Press Ctrl+C to stop the server.\n\n"
    );


    /* =========================================
       STEP 7
       Main server loop
       ========================================= */

    while (1)
    {
        clientAddressSize =
            sizeof(clientAddress);


        /*
            Wait for browser connection
        */

        clientSocket = accept(
            serverSocket,
            (struct sockaddr *)&clientAddress,
            &clientAddressSize
        );


        if (clientSocket == INVALID_SOCKET)
        {
            printf(
                "Client connection failed.\n"
            );

            continue;
        }


        /*
            Clear old request data
        */

        memset(
            request,
            0,
            sizeof(request)
        );


        /*
            Receive HTTP request
        */

        recv(
            clientSocket,
            request,
            sizeof(request) - 1,
            0
        );


        printf("\nRequest received:\n");
        printf("-----------------------------\n");

        printf("%s\n", request);

        printf("-----------------------------\n");


        /* =====================================
           ROUTE 1

           GET /test

           Used to check whether the
           C server is running.
           ===================================== */

        if (
            strncmp(
                request,
                "GET /test ",
                10
            ) == 0
        )
        {
            sendResponse(
                clientSocket,
                "RoomSync C Server is working!"
            );
        }


        /* =====================================
           ROUTE 2

           GET /rooms

           Returns all rooms and their
           CURRENT database status.
           ===================================== */

        else if (
            strncmp(
                request,
                "GET /rooms ",
                11
            ) == 0
        )
        {
            char roomsJSON[4096];


            if (
                getAllRoomsJSON(
                    db,
                    roomsJSON,
                    sizeof(roomsJSON)
                )
            )
            {
                sendResponse(
                    clientSocket,
                    roomsJSON
                );
            }

            else
            {
                sendResponse(
                    clientSocket,
                    "[]"
                );
            }
        }


        /* =====================================
           ROUTE 3

           FLOOR MANAGER STATUS UPDATE

           Example:

           GET /room-status?
           room=101&status=OCCUPIED

           Actual URL:

           /room-status?room=101&status=OCCUPIED
           ===================================== */

        else if (
            strncmp(
                request,
                "GET /room-status?",
                17
            ) == 0
        )
        {
            char roomNumber[20];

            char status[20];


            /*
                Read room number and status
                from the URL.
            */

            if (
                sscanf(
                    request,

                    "GET /room-status?"
                    "room=%19[^&]"
                    "&status=%19s",

                    roomNumber,
                    status
                ) == 2
            )
            {
                char *space;


                /*
                    HTTP request contains:

                    OCCUPIED HTTP/1.1

                    We only want:

                    OCCUPIED

                    So remove everything
                    after the first space.
                */

                space = strchr(
                    status,
                    ' '
                );


                if (space != NULL)
                {
                    *space = '\0';
                }


                printf(
                    "\nFloor Manager Request\n"
                );

                printf(
                    "Room: %s\n",
                    roomNumber
                );

                printf(
                    "Status: %s\n",
                    status
                );


                /*
                    Update SQLite database
                */

                if (
                    updateRoomStatus(
                        db,
                        roomNumber,
                        status
                    )
                )
                {
                    sendResponse(
                        clientSocket,
                        "Room status updated successfully."
                    );
                }

                else
                {
                    sendResponse(
                        clientSocket,
                        "Could not update room status."
                    );
                }
            }

            else
            {
                sendResponse(
                    clientSocket,
                    "Invalid room status request."
                );
            }
        }


        /* =====================================
           UNKNOWN ROUTE
           ===================================== */

        else
        {
            sendResponse(
                clientSocket,
                "RoomSync server: route not found."
            );
        }


        /*
            Finished with this browser request.
        */

        closesocket(clientSocket);
    }


    /*
        These lines normally run when
        the server shuts down.
    */

    closesocket(serverSocket);

    closeDatabase(db);

    WSACleanup();


    return 0;
}