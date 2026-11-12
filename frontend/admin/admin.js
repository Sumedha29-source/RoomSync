
/* =========================================
   ROOMSYNC ADMIN DASHBOARD
   Connected to C Backend + SQLite
   ========================================= */

const API_BASE = "http://127.0.0.1:8080";

let rooms = [];


/* =========================================
   NAVIGATION
   ========================================= */

const navigationButtons =
    document.querySelectorAll(".nav-item");


function openPage(page)
{
    document.querySelectorAll(".page").forEach(function(element)
    {
        element.classList.remove("active-page");
    });

    navigationButtons.forEach(function(button)
    {
        button.classList.remove("active");

        if (button.dataset.page === page)
        {
            button.classList.add("active");
        }
    });

    const selectedPage =
        document.getElementById(page + "Page");

    if (selectedPage)
    {
        selectedPage.classList.add("active-page");
    }

    const titles = {
        dashboard: "Dashboard",
        rooms: "Rooms",
        departments: "Departments",
        timetables: "Timetables",
        managers: "Floor Managers",
        import: "Import Routine"
    };

    document.getElementById("pageTitle").textContent =
        titles[page] || "RoomSync Admin";
}


navigationButtons.forEach(function(button)
{
    button.addEventListener("click", function()
    {
        openPage(button.dataset.page);
    });
});


/* =========================================
   SERVER CONNECTION
   ========================================= */

async function apiRequest(route, method = "GET", body = null)
{
    const response = await fetch(
        API_BASE + route,
        {
            method: method,
            cache: "no-store",
            credentials: "include",

            headers: body
                ? {
                    "Content-Type": "application/x-www-form-urlencoded"
                }
                : {},

            body: body
        }
    );

    const result = await response.text();

    if (!response.ok)
    {
        throw new Error(result || "Server request failed.");
    }

    return result;
}


/* =========================================
   LOAD ROOMS FROM DATABASE
   ========================================= */

async function loadRooms()
{
    const serverStatus =
        document.getElementById("serverStatus");

    try
    {
        const result = await apiRequest("/rooms");

        rooms = JSON.parse(result);

        if (!Array.isArray(rooms))
        {
            throw new Error("Invalid room data received.");
        }

        serverStatus.textContent = "Server Connected";
        serverStatus.className = "server-status online";

        updateDashboard();
        displayRooms();
    }

    catch (error)
    {
        console.error("RoomSync error:", error);

        serverStatus.textContent = "Server Offline";
        serverStatus.className = "server-status offline";

        const tableBody =
            document.getElementById("roomsTableBody");

        tableBody.innerHTML = "";

        const row = document.createElement("tr");
        const cell = document.createElement("td");

        cell.colSpan = 5;
        cell.className = "table-message";
        cell.textContent = error.message;

        row.appendChild(cell);
        tableBody.appendChild(row);

        document.getElementById("totalRooms").textContent = "-";
        document.getElementById("emptyRooms").textContent = "-";
        document.getElementById("occupiedRooms").textContent = "-";
        document.getElementById("totalFloors").textContent = "-";
    }
}


/* =========================================
   DASHBOARD STATISTICS
   ========================================= */

function updateDashboard()
{
    let emptyRooms = 0;
    let occupiedRooms = 0;

    const floors = new Set();

    rooms.forEach(function(room)
    {
        floors.add(room.floor);

        if (room.status === "EMPTY")
        {
            emptyRooms++;
        }

        else if (room.status === "OCCUPIED")
        {
            occupiedRooms++;
        }
    });

    document.getElementById("totalRooms").textContent =
        rooms.length;

    document.getElementById("emptyRooms").textContent =
        emptyRooms;

    document.getElementById("occupiedRooms").textContent =
        occupiedRooms;

    document.getElementById("totalFloors").textContent =
        floors.size;
}


/* =========================================
   DISPLAY ROOMS
   ========================================= */

function displayRooms()
{
    const tableBody =
        document.getElementById("roomsTableBody");

    tableBody.innerHTML = "";

    if (rooms.length === 0)
    {
        const row = document.createElement("tr");
        const cell = document.createElement("td");

        cell.colSpan = 5;
        cell.className = "table-message";
        cell.textContent = "No rooms registered.";

        row.appendChild(cell);
        tableBody.appendChild(row);

        return;
    }

    rooms.forEach(function(room)
    {
        const roomNumber =
            room.room_number || room["room number"];

        const row = document.createElement("tr");

        const nameCell = document.createElement("td");
        nameCell.className = "room-name";
        nameCell.textContent = "Room " + roomNumber;

        const floorCell = document.createElement("td");
        floorCell.textContent = room.floor;

        const capacityCell = document.createElement("td");
        capacityCell.textContent = room.capacity;

        const statusCell = document.createElement("td");

        const statusBadge = document.createElement("span");

        statusBadge.className =
            room.status === "EMPTY"
                ? "status-badge status-empty"
                : "status-badge status-occupied";

        statusBadge.textContent = room.status || "UNKNOWN";

        statusCell.appendChild(statusBadge);

        const actionCell = document.createElement("td");

        const editButton = document.createElement("button");

        editButton.className = "action-button";
        editButton.textContent = "Edit";

        editButton.addEventListener("click", function()
        {
            editRoom(roomNumber);
        });

        const deleteButton = document.createElement("button");

        deleteButton.className = "action-button";
        deleteButton.textContent = "Delete";
        deleteButton.style.marginLeft = "6px";

        deleteButton.addEventListener("click", function()
        {
            deleteRoom(roomNumber);
        });

        actionCell.appendChild(editButton);
        actionCell.appendChild(deleteButton);

        row.appendChild(nameCell);
        row.appendChild(floorCell);
        row.appendChild(capacityCell);
        row.appendChild(statusCell);
        row.appendChild(actionCell);

        tableBody.appendChild(row);
    });
}


/* =========================================
   VALIDATE ROOM INPUT
   ========================================= */

function validRoomInput(room, floor, capacity)
{
    if (!room || !room.trim())
    {
        alert("Enter a valid room number.");
        return false;
    }

    if (!Number.isInteger(floor) || floor <= 0)
    {
        alert("Floor must be a positive whole number.");
        return false;
    }

    if (!Number.isInteger(capacity) || capacity <= 0)
    {
        alert("Capacity must be a positive whole number.");
        return false;
    }

    return true;
}


/* =========================================
   ADD ROOM
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

    if (!validRoomInput(room, floor, capacity))
    {
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

        const message = await apiRequest(
            "/room-add",
            "POST",
            params
        );

        alert(message);

        await loadRooms();
    }
    catch (error)
    {
        alert("Could not add room: " + error.message);
    }
}

/* =========================================
   EDIT ROOM
   ========================================= */

async function editRoom(oldRoom)
{
    const existingRoom = rooms.find(function(room)
    {
        return String(room.room_number || room["room number"]) ===
            String(oldRoom);
    });

    if (!existingRoom) return;

    const newRoom = prompt(
        "Enter new room number:",
        oldRoom
    );

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

    const floorInput = prompt(
        "Enter floor:",
        existingRoom.floor
    );

    if (floorInput === null) return;

    const capacityInput = prompt(
        "Enter capacity:",
        existingRoom.capacity
    );

    if (capacityInput === null) return;

    const floor = Number(floorInput);
    const capacity = Number(capacityInput);

    if (!validRoomInput(newRoom, floor, capacity))
    {
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

        const message = await apiRequest(
            "/room-edit",
            "POST",
            params
        );

        alert(message);

        await loadRooms();
    }
    catch (error)
    {
        alert("Could not edit room: " + error.message);
    }
}

/* =========================================
   DELETE ROOM
   ========================================= */

async function deleteRoom(roomNumber)
{
    const confirmed = confirm(
        "Are you sure you want to delete Room " +
        roomNumber + "?"
    );

    if (!confirmed) return;

    try
    {
        const params = new URLSearchParams({
            room: roomNumber
        });

        const message = await apiRequest(
            "/room-delete",
            "POST",
            params
        );

        alert(message);

        await loadRooms();
    }
    catch (error)
    {
        alert("Could not delete room: " + error.message);
    }
}


/* =========================================
   BUTTON EVENTS
   ========================================= */

document.getElementById("addRoomButton")
    .addEventListener("click", addRoom);


/* =========================================
   INITIAL LOAD
   ========================================= */

window.addEventListener("load", function()
{
    checkAdminSession();
});

/* =========================================
   ADMIN LOGIN
   ========================================= */

const loginScreen = document.getElementById("loginScreen");

const adminLoginForm = document.getElementById("adminLoginForm");


adminLoginForm.addEventListener("submit", async function(event)
{
    event.preventDefault();

    const username = document.getElementById("adminUsername").value;
    const password = document.getElementById("adminPassword").value;

    const message = document.getElementById("loginMessage");

    message.textContent = "Logging in...";

    try
    {
        const body = new URLSearchParams({
            username: username,
            password: password
        });

        const result = await apiRequest(
            "/login",
            "POST",
            body
        );

        const data = JSON.parse(result);

        if (data.role !== "ADMIN")
        {
            throw new Error("Admin access only.");
        }

        loginScreen.style.display = "none";

        loadRooms();
    }
    catch (error)
    {
        message.textContent = error.message;
    }
});


/* =========================================
   CHECK EXISTING SESSION
   ========================================= */

async function checkAdminSession()
{
    try
    {
        const result = await apiRequest("/session");

        const data = JSON.parse(result);

        if (data.authenticated && data.role === "ADMIN")
        {
            loginScreen.style.display = "none";
            loadRooms();
        }
        else
        {
            loginScreen.style.display = "flex";
        }
    }
    catch (error)
    {
        loginScreen.style.display = "flex";
    }
}