/*
    RoomSync Floor Manager
    ----------------------
    This file does two things:

    1. Loads the current room statuses from SQLite
       through the C server.

    2. Allows the floor manager to change a room's
       physical status to EMPTY or OCCUPIED.
*/


/* =========================================
   LOAD CURRENT ROOM STATUS
   ========================================= */

function loadRoomStatuses()
{
    fetch("http://localhost:8080/rooms")

        .then(function(response)
        {
            return response.json();
        })

        .then(function(rooms)
        {
            console.log("Rooms received:", rooms);


            /*
                Go through every room returned
                by the C server.
            */

            rooms.forEach(function(room)
            {
                const statusElement =
                    document.getElementById(
                        "status-" + room.room_number
                    );


                /*
                    Only update the element if
                    that room exists on this page.
                */

                if (statusElement)
                {
                    statusElement.textContent =
                        room.status;
                }
            });
        })

        .catch(function(error)
        {
            console.error(
                "Could not load room statuses:",
                error
            );
        });
}



/* =========================================
   UPDATE ROOM STATUS
   ========================================= */

function updateStatus(roomNumber, status)
{
    const url =
        "http://localhost:8080/room-status" +
        "?room=" +
        encodeURIComponent(roomNumber) +
        "&status=" +
        encodeURIComponent(status);


    fetch(url)

        .then(function(response)
        {
            return response.text();
        })

        .then(function(message)
        {
            console.log(message);


            /*
                Update the status shown
                on the webpage.
            */

            const statusElement =
                document.getElementById(
                    "status-" + roomNumber
                );


            if (statusElement)
            {
                statusElement.textContent =
                    status;
            }
        })

        .catch(function(error)
        {
            console.error(
                "Could not update room status:",
                error
            );

            alert(
                "Could not connect to RoomSync server."
            );
        });
}



/* =========================================
   LOAD DATABASE STATUS WHEN PAGE OPENS
   ========================================= */

window.onload = function()
{
    loadRoomStatuses();
};