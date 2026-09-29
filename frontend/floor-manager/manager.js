/*
    RoomSync Floor Manager Dashboard

    Rooms are NOT hard-coded into HTML.

    This page:
    1. Gets every room from SQLite.
    2. Groups rooms automatically by floor.
    3. Creates the dashboard.
    4. Updates physical occupancy through the C server.
*/


const API_BASE =
    "http://localhost:8080";


const floorsContainer =
    document.getElementById("floorsContainer");

const totalRoomsElement =
    document.getElementById("totalRooms");

const emptyRoomsElement =
    document.getElementById("emptyRooms");

const occupiedRoomsElement =
    document.getElementById("occupiedRooms");

const connectionStatus =
    document.getElementById("connectionStatus");

const refreshButton =
    document.getElementById("refreshButton");


/* =========================================
   LOAD ALL ROOMS
   ========================================= */

function loadRooms()
{
    connectionStatus.textContent =
        "Loading...";

    connectionStatus.className =
        "connection-status";


    fetch(API_BASE + "/rooms")

        .then(function(response)
        {
            if (!response.ok)
            {
                throw new Error(
                    "Server returned an error."
                );
            }


            return response.json();
        })


        .then(function(rooms)
        {
            connectionStatus.textContent =
                "Server Connected";

            connectionStatus.className =
                "connection-status connected";


            updateStatistics(rooms);

            displayRoomsByFloor(rooms);
        })


        .catch(function(error)
        {
            console.error(
                "Could not load rooms:",
                error
            );


            connectionStatus.textContent =
                "Server Offline";

            connectionStatus.className =
                "connection-status error";


            totalRoomsElement.textContent =
                "-";

            emptyRoomsElement.textContent =
                "-";

            occupiedRoomsElement.textContent =
                "-";


            floorsContainer.innerHTML = `

                <div class="loading-state">

                    <div class="loading-icon">
                        !
                    </div>

                    <h3>
                        Could not load classrooms
                    </h3>

                    <p>
                        Make sure the RoomSync C server
                        is running on port 8080.
                    </p>

                </div>
            `;
        });
}



/* =========================================
   STATISTICS
   ========================================= */

function updateStatistics(rooms)
{
    let emptyCount = 0;

    let occupiedCount = 0;


    rooms.forEach(function(room)
    {
        if (room.status === "EMPTY")
        {
            emptyCount++;
        }

        else if (room.status === "OCCUPIED")
        {
            occupiedCount++;
        }
    });


    totalRoomsElement.textContent =
        rooms.length;

    emptyRoomsElement.textContent =
        emptyCount;

    occupiedRoomsElement.textContent =
        occupiedCount;
}



/* =========================================
   GROUP AND DISPLAY ROOMS
   ========================================= */

function displayRoomsByFloor(rooms)
{
    floorsContainer.innerHTML = "";


    if (rooms.length === 0)
    {
        floorsContainer.innerHTML = `

            <div class="loading-state">

                <div class="loading-icon">
                    0
                </div>

                <h3>
                    No classrooms found
                </h3>

                <p>
                    There are currently no rooms
                    stored in the RoomSync database.
                </p>

            </div>
        `;

        return;
    }


    /*
        Object will look like:

        {
            1: [room101, room102],
            2: [room201, room202]
        }
    */

    const floors = {};


    rooms.forEach(function(room)
    {
        if (!floors[room.floor])
        {
            floors[room.floor] = [];
        }


        floors[room.floor].push(room);
    });


    /*
        Sort floor numbers.

        This means if the database contains:
        Floor 3, Floor 1, Floor 2

        the webpage displays:
        Floor 1, Floor 2, Floor 3
    */

    const floorNumbers =
        Object.keys(floors).sort(
            function(a, b)
            {
                return Number(a) - Number(b);
            }
        );


    floorNumbers.forEach(function(floorNumber)
    {
        createFloorSection(
            floorNumber,
            floors[floorNumber]
        );
    });
}



/* =========================================
   CREATE FLOOR
   ========================================= */

function createFloorSection(
    floorNumber,
    rooms
)
{
    const floorSection =
        document.createElement("div");


    floorSection.className =
        "floor-section";


    const floorHeading =
        document.createElement("div");


    floorHeading.className =
        "floor-heading";


    floorHeading.innerHTML = `

        <h3>
            Floor ${floorNumber}
        </h3>

        <span class="floor-room-count">
            ${rooms.length}
            ${rooms.length === 1 ? "room" : "rooms"}
        </span>
    `;


    const floorRooms =
        document.createElement("div");


    floorRooms.className =
        "floor-rooms";


    rooms.forEach(function(room)
    {
        const roomRow =
            createRoomRow(room);


        floorRooms.appendChild(
            roomRow
        );
    });


    floorSection.appendChild(
        floorHeading
    );


    floorSection.appendChild(
        floorRooms
    );


    floorsContainer.appendChild(
        floorSection
    );
}



/* =========================================
   CREATE INDIVIDUAL ROOM
   ========================================= */

function createRoomRow(room)
{
    const roomRow =
        document.createElement("div");


    roomRow.className =
        "room-row";


    roomRow.id =
        "room-" + room.room_number;


    const isEmpty =
        room.status === "EMPTY";


    roomRow.innerHTML = `

        <div class="room-info">

            <h4>
                Room ${room.room_number}
            </h4>

            <p>
                Capacity: ${room.capacity}
            </p>

        </div>


        <div>

            <span
                id="status-${room.room_number}"
                class="
                    room-status
                    ${isEmpty
                        ? "status-empty"
                        : "status-occupied"}
                "
            >

                <span class="status-dot"></span>

                ${room.status}

            </span>

        </div>


        <div class="status-buttons">

            <button
                id="empty-${room.room_number}"
                class="
                    status-button
                    empty-button
                    ${isEmpty ? "active" : ""}
                "
                onclick="
                    updateStatus(
                        '${room.room_number}',
                        'EMPTY'
                    )
                "
            >
                Empty
            </button>


            <button
                id="occupied-${room.room_number}"
                class="
                    status-button
                    occupied-button
                    ${!isEmpty ? "active" : ""}
                "
                onclick="
                    updateStatus(
                        '${room.room_number}',
                        'OCCUPIED'
                    )
                "
            >
                Occupied
            </button>

        </div>
    `;


    return roomRow;
}



/* =========================================
   UPDATE ROOM STATUS
   ========================================= */

function updateStatus(
    roomNumber,
    status
)
{
    const emptyButton =
        document.getElementById(
            "empty-" + roomNumber
        );


    const occupiedButton =
        document.getElementById(
            "occupied-" + roomNumber
        );


    /*
        Disable both buttons while
        database update is happening.
    */

    emptyButton.disabled = true;

    occupiedButton.disabled = true;


    const url =
        API_BASE +
        "/room-status" +

        "?room=" +
        encodeURIComponent(roomNumber) +

        "&status=" +
        encodeURIComponent(status);


    fetch(url)

        .then(function(response)
        {
            if (!response.ok)
            {
                throw new Error(
                    "Server returned an error."
                );
            }


            return response.text();
        })


        .then(function(message)
        {
            console.log(message);


            /*
                Reload from the DATABASE instead
                of manually assuming the update
                worked.

                This keeps the webpage synchronized
                with SQLite.
            */

            loadRooms();
        })


        .catch(function(error)
        {
            console.error(
                "Could not update room:",
                error
            );


            alert(
                "Could not update Room " +
                roomNumber +
                "."
            );


            emptyButton.disabled = false;

            occupiedButton.disabled = false;
        });
}



/* =========================================
   MANUAL REFRESH BUTTON
   ========================================= */

refreshButton.addEventListener(
    "click",
    function()
    {
        loadRooms();
    }
);



/* =========================================
   INITIAL PAGE LOAD
   ========================================= */

window.addEventListener(
    "load",
    function()
    {
        loadRooms();
    }
);