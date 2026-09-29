/*
    RoomSync
    Student Classroom Search

    Frontend:
    HTML + CSS + JavaScript

    Backend:
    C + SQLite
*/


const searchButton =
    document.getElementById("searchButton");

const roomResults =
    document.getElementById("roomResults");

const resultCount =
    document.getElementById("resultCount");


/* =========================================
   SEARCH BUTTON
   ========================================= */

searchButton.addEventListener(
    "click",
    function ()
    {
        const day =
            document.getElementById("day").value;

        const startTime =
            document.getElementById("startTime").value;

        const endTime =
            document.getElementById("endTime").value;


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


        if (endTime <= startTime)
        {
            alert(
                "End time must be later than start time."
            );

            return;
        }


        /* =====================================
           LOADING STATE
           ===================================== */

        resultCount.classList.add(
            "hidden"
        );


        roomResults.className = "";


        roomResults.innerHTML = `
            <div class="empty-state">

                <div class="empty-icon">
                    ...
                </div>

                <h3>
                    Checking classrooms
                </h3>

                <p>
                    Comparing the college routine
                    with current room occupancy.
                </p>

            </div>
        `;


        /* =====================================
           BUILD API URL
           ===================================== */

        const url =
            "http://localhost:8080/available-rooms" +
            "?day=" +
            encodeURIComponent(day) +

            "&start=" +
            encodeURIComponent(startTime) +

            "&end=" +
            encodeURIComponent(endTime);


        /* =====================================
           CONTACT C SERVER
           ===================================== */

        fetch(url)

            .then(function (response)
            {
                if (!response.ok)
                {
                    throw new Error(
                        "Server returned an error."
                    );
                }


                return response.json();
            })


            .then(function (rooms)
            {
                displayRooms(rooms);
            })


            .catch(function (error)
            {
                console.error(
                    "Room search failed:",
                    error
                );


                resultCount.classList.add(
                    "hidden"
                );


                roomResults.className = "";


                roomResults.innerHTML = `
                    <div class="empty-state">

                        <div class="empty-icon">
                            !
                        </div>

                        <h3>
                            Server unavailable
                        </h3>

                        <p>
                            RoomSync could not connect
                            to the C server. Make sure
                            server.exe is running.
                        </p>

                    </div>
                `;
            });
    }
);



/* =========================================
   DISPLAY AVAILABLE ROOMS
   ========================================= */

function displayRooms(rooms)
{
    roomResults.innerHTML = "";


    /* =====================================
       NO AVAILABLE ROOMS
       ===================================== */

    if (rooms.length === 0)
    {
        roomResults.className = "";


        resultCount.textContent =
            "0 rooms";


        resultCount.classList.remove(
            "hidden"
        );


        roomResults.innerHTML = `
            <div class="empty-state">

                <div class="empty-icon">
                    0
                </div>

                <h3>
                    No rooms available
                </h3>

                <p>
                    No classroom is currently
                    available for the selected
                    time slot.
                </p>

            </div>
        `;


        return;
    }


    /* =====================================
       RESULT COUNT
       ===================================== */

    if (rooms.length === 1)
    {
        resultCount.textContent =
            "1 room";
    }

    else
    {
        resultCount.textContent =
            rooms.length + " rooms";
    }


    resultCount.classList.remove(
        "hidden"
    );


    /* =====================================
       ROOM GRID
       ===================================== */

    roomResults.className =
        "room-grid";


    rooms.forEach(function (room)
    {
        const roomCard =
            document.createElement("div");


        roomCard.className =
            "room-card";


        roomCard.innerHTML = `

            <div class="room-status">

                <span class="status-dot"></span>

                AVAILABLE

            </div>


            <h3>
                Room ${room.room_number}
            </h3>


            <div class="room-details">

                <div class="room-detail">

                    <span>
                        Floor
                    </span>

                    <strong>
                        ${room.floor}
                    </strong>

                </div>


                <div class="room-detail">

                    <span>
                        Capacity
                    </span>

                    <strong>
                        ${room.capacity}
                    </strong>

                </div>

            </div>
        `;


        roomResults.appendChild(
            roomCard
        );
    });
}