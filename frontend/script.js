/*
    RoomSync Student Search

    This file sends the student's selected
    day and time to the C backend.

    The C backend checks:

    1. Timetable / routine
    2. Floor Manager physical room status

    Only genuinely available rooms are returned.
*/


const searchButton =
    document.getElementById("searchButton");


searchButton.addEventListener(
    "click",
    function ()
    {
        /* Get values entered by the student */

        const day =
            document.getElementById("day").value;

        const startTime =
            document.getElementById("startTime").value;

        const endTime =
            document.getElementById("endTime").value;

        const roomResults =
            document.getElementById("roomResults");


        /* =====================================
           VALIDATION
           ===================================== */

        if (
            day === "" ||
            startTime === "" ||
            endTime === ""
        )
        {
            alert(
                "Please select the day, start time and end time."
            );

            return;
        }


        /*
            End time must be later
            than start time.
        */

        if (endTime <= startTime)
        {
            alert(
                "End time must be later than start time."
            );

            return;
        }


        /* Show temporary message */

        roomResults.innerHTML =
            "<p>Searching for available rooms...</p>";


        /* =====================================
           CREATE BACKEND URL
           ===================================== */

        const url =
            "http://localhost:8080/available-rooms" +
            "?day=" +
            encodeURIComponent(day) +
            "&start=" +
            encodeURIComponent(startTime) +
            "&end=" +
            encodeURIComponent(endTime);


        console.log(
            "Searching:",
            url
        );


        /* =====================================
           SEND REQUEST TO C SERVER
           ===================================== */

        fetch(url)

            .then(function (response)
            {
                /*
                    Convert the JSON returned
                    by the C server into a
                    JavaScript array.
                */

                return response.json();
            })


            .then(function (rooms)
            {
                console.log(
                    "Available rooms:",
                    rooms
                );


                /* Clear searching message */

                roomResults.innerHTML = "";


                /* =================================
                   NO ROOMS AVAILABLE
                   ================================= */

                if (rooms.length === 0)
                {
                    roomResults.innerHTML =
                        "<p>No rooms are available for this time.</p>";

                    return;
                }


                /* =================================
                   DISPLAY AVAILABLE ROOMS
                   ================================= */

                rooms.forEach(function (room)
                {
                    /*
                        Create one card for
                        each available room.
                    */

                    const roomCard =
                        document.createElement("div");


                    roomCard.className =
                        "room-card";


                    roomCard.innerHTML =
                        "<h3>Room " +
                        room.room_number +
                        "</h3>" +

                        "<p>Floor: " +
                        room.floor +
                        "</p>" +

                        "<p>Capacity: " +
                        room.capacity +
                        "</p>" +

                        "<p class='available-status'>" +
                        "Available" +
                        "</p>";


                    roomResults.appendChild(
                        roomCard
                    );
                });
            })


            .catch(function (error)
            {
                console.error(
                    "Room search failed:",
                    error
                );


                roomResults.innerHTML =
                    "<p>Could not connect to the RoomSync server.</p>";


                alert(
                    "Could not connect to RoomSync server. Make sure server.exe is running."
                );
            });
    }
);