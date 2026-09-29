/*
    RoomSync Admin Dashboard

    Current functionality:

    - Navigation
    - Server connection status
    - Room statistics
    - Floor statistics
    - View rooms from SQLite

    Add/Edit/Delete functionality will be
    connected to the C backend next.
*/


const API_BASE =
    "http://localhost:8080";


let rooms = [];


/* =========================================
   NAVIGATION
   ========================================= */

const navigationButtons =
    document.querySelectorAll(".nav-item");


navigationButtons.forEach(function(button)
{
    button.addEventListener(
        "click",
        function()
        {
            const page =
                button.dataset.page;


            openPage(page);
        }
    );
});


function openPage(page)
{
    const pages =
        document.querySelectorAll(".page");


    pages.forEach(function(pageElement)
    {
        pageElement.classList.remove(
            "active-page"
        );
    });


    navigationButtons.forEach(function(button)
    {
        button.classList.remove(
            "active"
        );


        if (button.dataset.page === page)
        {
            button.classList.add(
                "active"
            );
        }
    });


    const selectedPage =
        document.getElementById(
            page + "Page"
        );


    if (selectedPage)
    {
        selectedPage.classList.add(
            "active-page"
        );
    }


    const titles =
    {
        dashboard: "Dashboard",
        rooms: "Rooms",
        departments: "Departments",
        timetables: "Timetables",
        managers: "Floor Managers",
        import: "Import Routine"
    };


    document.getElementById(
        "pageTitle"
    ).textContent =
        titles[page] || "RoomSync Admin";
}



/* =========================================
   LOAD ROOM DATA
   ========================================= */

function loadRooms()
{
    const serverStatus =
        document.getElementById(
            "serverStatus"
        );


    fetch(API_BASE + "/rooms")

        .then(function(response)
        {
            if (!response.ok)
            {
                throw new Error(
                    "Server error"
                );
            }


            return response.json();
        })


        .then(function(data)
        {
            rooms = data;


            serverStatus.textContent =
                "Server Connected";

            serverStatus.className =
                "server-status online";


            updateDashboard();

            displayRooms();
        })


        .catch(function(error)
        {
            console.error(
                "Could not load RoomSync:",
                error
            );


            serverStatus.textContent =
                "Server Offline";

            serverStatus.className =
                "server-status offline";


            document.getElementById(
                "roomsTableBody"
            ).innerHTML = `

                <tr>

                    <td
                        colspan="5"
                        class="table-message"
                    >
                        Could not connect to RoomSync server.
                    </td>

                </tr>
            `;
        });
}



/* =========================================
   DASHBOARD STATISTICS
   ========================================= */

function updateDashboard()
{
    let emptyRooms = 0;

    let occupiedRooms = 0;

    const floors =
        new Set();


    rooms.forEach(function(room)
    {
        floors.add(
            room.floor
        );


        if (room.status === "EMPTY")
        {
            emptyRooms++;
        }

        else if (
            room.status === "OCCUPIED"
        )
        {
            occupiedRooms++;
        }
    });


    document.getElementById(
        "totalRooms"
    ).textContent =
        rooms.length;


    document.getElementById(
        "emptyRooms"
    ).textContent =
        emptyRooms;


    document.getElementById(
        "occupiedRooms"
    ).textContent =
        occupiedRooms;


    document.getElementById(
        "totalFloors"
    ).textContent =
        floors.size;
}



/* =========================================
   DISPLAY ROOMS
   ========================================= */

function displayRooms()
{
    const tableBody =
        document.getElementById(
            "roomsTableBody"
        );


    tableBody.innerHTML = "";


    if (rooms.length === 0)
    {
        tableBody.innerHTML = `

            <tr>

                <td
                    colspan="5"
                    class="table-message"
                >
                    No rooms are currently registered.
                </td>

            </tr>
        `;


        return;
    }


    rooms.forEach(function(room)
    {
        const row =
            document.createElement("tr");


        const statusClass =
            room.status === "EMPTY"
                ? "status-empty"
                : "status-occupied";


        row.innerHTML = `

            <td class="room-name">
                Room ${room.room_number}
            </td>


            <td>
                ${room.floor}
            </td>


            <td>
                ${room.capacity}
            </td>


            <td>

                <span
                    class="
                        status-badge
                        ${statusClass}
                    "
                >
                    ${room.status}
                </span>

            </td>


            <td>

                <button
                    class="action-button"
                    onclick="
                        editRoom(
                            '${room.room_number}'
                        )
                    "
                >
                    Edit
                </button>

            </td>
        `;


        tableBody.appendChild(
            row
        );
    });
}



/* =========================================
   FUTURE ROOM FUNCTIONS
   ========================================= */

function editRoom(roomNumber)
{
    alert(
        "Room editing will be connected next.\n\n" +
        "Selected Room: " +
        roomNumber
    );
}


document.getElementById(
    "addRoomButton"
).addEventListener(
    "click",
    function()
    {
        alert(
            "Add Room will be connected to SQLite next."
        );
    }
);



/* =========================================
   INITIAL LOAD
   ========================================= */

window.addEventListener(
    "load",
    function()
    {
        loadRooms();
    }
);