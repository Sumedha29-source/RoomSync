/*
    RoomSync Floor Manager Dashboard

    Rooms are NOT hard-coded into HTML.

    This page:
    1. Gets every room from SQLite.
    2. Groups rooms automatically by floor.
    3. Creates the dashboard.
    4. Updates physical occupancy through the C server.
*/


const API_BASE = "http://127.0.0.1:8080";


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

const loginScreen = document.getElementById("loginScreen");
const managerLoginForm = document.getElementById("managerLoginForm");
const loginMessage = document.getElementById("loginMessage");
const managerLoginSection = document.getElementById("managerLoginSection");

const managerPasswordSection = document.getElementById("managerPasswordSection");

const managerPasswordForm = document.getElementById("managerPasswordForm");

const passwordChangeMessage = document.getElementById("passwordChangeMessage");

/* =========================================
   API REQUEST HELPER
   ========================================= */

async function apiRequest(route, method = "GET", body = null)
{
    const response = await fetch(API_BASE + route, {
        method: method,
        cache: "no-store",
        credentials: "include",

        headers: body
            ? {
                "Content-Type": "application/x-www-form-urlencoded"
            }
            : {},

        body: body
    });

    const result = await response.text();

    if (!response.ok)
    {
        throw new Error(result || "Server request failed.");
    }

    return result;
}

/* =========================================
   FLOOR MANAGER LOGIN
   ========================================= */


managerLoginForm.addEventListener("submit", async function(event)
{
    event.preventDefault();

    const username = document.getElementById("managerUsername").value;
    const password = document.getElementById("managerPassword").value;

    loginMessage.textContent = "Logging in...";

    try
    {
        const params = new URLSearchParams({
            username: username,
            password: password
        });

        const result = await apiRequest(
            "/login",
            "POST",
            params
        );

        const data = JSON.parse(result);

        if (data.role !== "MANAGER")
        {
            await apiRequest("/logout", "POST");
            throw new Error("Floor Manager access only.");
        }

        managerLoginForm.reset();

        if (data.mustChangePassword)
        {
            managerLoginSection.style.display = "none";
            managerPasswordSection.style.display = "block";
            passwordChangeMessage.textContent = "";
        }
        else
        {
            loginScreen.style.display = "none";
            loadRooms();
        }
    }
    catch(error)
    {
        loginMessage.textContent = error.message;
    }
});


/* =========================================
   CHANGE FLOOR MANAGER PASSWORD
   ========================================= */

managerPasswordForm.addEventListener("submit", async function(event)
{
    event.preventDefault();

    const oldPassword =
        document.getElementById("currentManagerPassword").value;

    const newPassword =
        document.getElementById("newManagerPassword").value;

    const confirmPassword =
        document.getElementById("confirmManagerPassword").value;

    if (newPassword !== confirmPassword)
    {
        passwordChangeMessage.textContent =
            "New passwords do not match.";
        return;
    }

    if (newPassword === oldPassword)
    {
        passwordChangeMessage.textContent =
            "New password must be different.";
        return;
    }

    passwordChangeMessage.textContent = "Updating password...";

    try
    {
        const params = new URLSearchParams({
            oldPassword: oldPassword,
            newPassword: newPassword
        });

        const result = await apiRequest(
            "/change-password",
            "POST",
            params
        );

        alert(result);

        managerPasswordForm.reset();
        loginScreen.style.display = "none";

        loadRooms();
    }
    catch(error)
    {
        passwordChangeMessage.textContent = error.message;
    }
});


/* =========================================
   CHECK MANAGER SESSION
   ========================================= */

async function checkManagerSession()
{
    try
    {
        const result = await apiRequest("/session");
        const data = JSON.parse(result);


if (data.authenticated && data.role === "MANAGER")
{
    if (data.mustChangePassword)
    {
        loginScreen.style.display = "flex";
        managerLoginSection.style.display = "none";
        managerPasswordSection.style.display = "block";
    }
    else
    {
        loginScreen.style.display = "none";
        loadRooms();
    }
}

        else
        {
            if (data.authenticated)
            {
                await apiRequest("/logout", "POST");
            }

            loginScreen.style.display = "flex";
        }
    }
    catch(error)
    {
        loginScreen.style.display = "flex";
        loginMessage.textContent = "Unable to verify login. Please try again.";
    }
}


/* =========================================
   LOAD ALL ROOMS
   ========================================= */

async function loadRooms()
{
    connectionStatus.textContent = "Loading...";
    connectionStatus.className = "connection-status";

    try
    {
        const result = await apiRequest("/manager-rooms");

        const rooms = JSON.parse(result);
        currentRooms = rooms;

        connectionStatus.textContent = "Server Connected";
        connectionStatus.className = "connection-status connected";

        updateStatistics(rooms);
        displayRoomsByFloor(rooms);
    }
    catch(error)
    {
        connectionStatus.textContent = "Server Offline";
        connectionStatus.className = "connection-status disconnected";

        floorsContainer.innerHTML =
            "<p>Unable to load assigned rooms.</p>";

        console.error(error);
    }
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
    const roomRow = document.createElement("div");

    roomRow.className = "room-row";
    roomRow.id = "room-" + room.room_number;

    const capacity = Number(room.capacity) || 0;
    const checked = room.status_updated_at != null &&
                    room.current_occupancy != null;

    const occupancy = checked
        ? Number(room.current_occupancy)
        : null;

    const isEmpty = checked && occupancy === 0;
    const isOccupied = checked && occupancy > 0;

    const actualStatus = !checked
        ? "NOT CHECKED"
        : isEmpty ? "EMPTY" : "OCCUPIED";

    const availableSeats = checked
        ? capacity - occupancy
        : null;

    roomRow.innerHTML = `

        <div class="room-info">
            <h4>Room ${room.room_number}</h4>
            <p>Capacity: ${capacity}</p>
            <p>
                Current students:
                ${checked ? occupancy : "Not checked"}
            </p>
            <p>
                Available seats:
                ${checked ? availableSeats : "Not checked"}
            </p>
        </div>

        <div>
            <span
                id="status-${room.room_number}"
                class="room-status ${
                    isEmpty ? "status-empty" :
                    isOccupied ? "status-occupied" : ""
                }"
            >
                <span class="status-dot"></span>
                ${actualStatus}
            </span>

            <p>
                Last checked:
                ${checked
                    ? (room.status_updated_at || "Unknown")
                    : "Not checked yet"}
            </p>
        </div>

        <div class="occupancy-control">

            <label>
                Students currently inside:
            </label>

            <input
                type="number"
                id="occupancy-${room.room_number}"
                min="0"
                max="${capacity}"
                value="${checked ? occupancy : 0}"
                placeholder="Enter count"
            >

            <p id="remaining-${room.room_number}">
                ${checked
                    ? availableSeats + " seats remaining"
                    : capacity + " seats available (unverified)"}
            </p>

            <div class="status-buttons">

                <button
                    class="status-button empty-button"
                    onclick="updateStatus('${room.room_number}', 'EMPTY')"
                >
                    Empty
                </button>

                <button
                    class="status-button occupied-button"
                    onclick="updateStatus('${room.room_number}', 'OCCUPIED')"
                >
                    Occupied
                </button>

            </div>

        </div>

        <div class="room-management-buttons">

            <button
                class="edit-room-button"
                onclick="editRoom('${room.room_number}')"
            >
                Edit
            </button>

            <button
                class="delete-room-button"
                onclick="deleteRoom('${room.room_number}')"
            >
                Delete
            </button>

        </div>
    `;

    const occupancyInput = roomRow.querySelector(
        `#occupancy-${room.room_number}`
    );

    const remainingText = roomRow.querySelector(
        `#remaining-${room.room_number}`
    );

    occupancyInput.addEventListener("input", function()
    {
        const count = Number(this.value);

        if (this.value === "")
        {
            remainingText.textContent = "Enter student count";
            return;
        }

        if (!Number.isInteger(count) ||
            count < 0 ||
            count > capacity)
        {
            remainingText.textContent =
                "Enter a count between 0 and " + capacity;
            return;
        }

        remainingText.textContent =
            (capacity - count) + " seats remaining";
    });

    return roomRow;
}


/* =========================================
   UPDATE ROOM STATUS AND OCCUPANCY
   ========================================= */

async function updateStatus(roomNumber, status)
{
    const occupancyInput = document.getElementById(
        "occupancy-" + roomNumber
    );

    const count = Number(occupancyInput.value);

    const room = currentRooms.find(
        r => r.room_number === roomNumber
    );

    if (!room)
    {
        alert("Room not found.");
        return;
    }

    const capacity = Number(room.capacity);

    if (
        occupancyInput.value === "" ||
        !Number.isInteger(count) ||
        count < 0 ||
        count > capacity
    )
    {
        alert("Enter a valid student count between 0 and " + capacity);
        return;
    }

    if (status === "EMPTY" && count !== 0)
    {
        alert("An empty room must have 0 students.");
        return;
    }

    if (status === "OCCUPIED" && count === 0)
    {
        alert("An occupied room must have at least 1 student.");
        return;
    }

    try
    {
        const params = new URLSearchParams({
            room: roomNumber,
            status: status,
            occupancy: String(count)
        });

        await apiRequest(
            "/room-status",
            "POST",
            params
        );

        alert("Room status and occupancy updated successfully.");

        await loadRooms();
    }
    catch(error)
    {
        alert("Could not update room status: " + error.message);
    }
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
        checkManagerSession();
    }
);

/* =========================================
   ROOM MANAGEMENT
   ========================================= */

async function addRoom()
{
    const room = prompt("Enter room number (e.g. ICT101):");
    if (room === null) return;

    const buildingInput = prompt("Enter building (ICT / CME / CB):");
    if (buildingInput === null) return;

    const building = buildingInput.trim().toUpperCase();

    if (!["ICT", "CME", "CB"].includes(building))
    {
        alert("Invalid building. Enter ICT, CME or CB.");
        return;
    }

    const floorInput = prompt("Enter floor number:");
    if (floorInput === null) return;

    const capacityInput = prompt("Enter room capacity:");
    if (capacityInput === null) return;

    const floor = Number(floorInput);
    const capacity = Number(capacityInput);

    if (!room.trim() || !Number.isInteger(floor) || floor < 0 ||
        !Number.isInteger(capacity) || capacity <= 0)
    {
        alert("Enter a valid room number, valid floor (0 or above) and positive capacity.");
        return;
    }

    try
    {
        const params = new URLSearchParams({
            room: room.trim(),
            building: building,
            floor: floor,
            capacity: capacity
        });

        const message = await apiRequest("/room-add", "POST", params);

        alert(message);
        await loadRooms();
    }
    catch (error)
    {
        alert("Could not add room: " + error.message);
    }
}


async function editRoom(oldRoom)
{
    const existingRoom = currentRooms.find(function(room)
    {
        return String(room.room_number) === String(oldRoom);
    });

    if (!existingRoom) return;

    const newRoom = prompt("Enter new room number:", oldRoom);
    if (newRoom === null) return;

    const buildingInput = prompt(
        "Enter building (ICT / CME / CB):",
        existingRoom.building || ""
    );
    if (buildingInput === null) return;

    const building = buildingInput.trim().toUpperCase();

    if (!["ICT", "CME", "CB"].includes(building))
    {
        alert("Invalid building. Enter ICT, CME or CB.");
        return;
    }

    const floorInput = prompt("Enter floor:", existingRoom.floor);
    if (floorInput === null) return;

    const capacityInput = prompt("Enter capacity:", existingRoom.capacity);
    if (capacityInput === null) return;

    const floor = Number(floorInput);
    const capacity = Number(capacityInput);

    if (!newRoom.trim() || !Number.isInteger(floor) || floor < 0 ||
        !Number.isInteger(capacity) || capacity <= 0)
    {
        alert("Enter a valid room number, valid floor (0 or above) and positive capacity.");
        return;
    }

    try
    {
        const params = new URLSearchParams({
            old: oldRoom,
            room: newRoom.trim(),
            building: building,
            floor: floor,
            capacity: capacity
        });

        const message = await apiRequest("/room-edit", "POST", params);

        alert(message);
        await loadRooms();
    }
    catch (error)
    {
        alert("Could not edit room: " + error.message);
    }
}


async function deleteRoom(roomNumber)
{
    const confirmed = confirm(
        "Are you sure you want to delete Room " + roomNumber + "?"
    );

    if (!confirmed) return;

    try
    {
        const params = new URLSearchParams({
            room: roomNumber
        });

        const message = await apiRequest("/room-delete", "POST", params);

        alert(message);
        await loadRooms();
    }
    catch (error)
    {
        alert("Could not delete room: " + error.message);
    }
}


let currentRooms = [];


document.getElementById("addRoomButton")
    .addEventListener("click", addRoom);