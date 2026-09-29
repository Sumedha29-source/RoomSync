# RoomSync

### Smart Classroom Availability Management System

RoomSync is a classroom availability management system designed to help students find classrooms that are actually available during a selected time slot.

Unlike a system that relies only on the academic timetable, RoomSync combines:

- Scheduled classroom routines
- Live physical occupancy status

A classroom is considered available only when:

```text
No scheduled class overlaps the requested time
                    AND
Physical room status = EMPTY
```

This helps prevent situations where a timetable shows a classroom as free even though the room is still physically occupied.

---

## Project Status

RoomSync is currently under active development.

### Implemented

- Student classroom search interface
- Search by day, start time, and end time
- Timetable conflict detection
- Physical room occupancy filtering
- Floor Manager dashboard
- Dynamic room generation from the database
- Floor-wise room grouping
- EMPTY / OCCUPIED room status updates
- Persistent room status using SQLite
- Admin dashboard interface
- Admin room overview
- Admin room statistics
- Responsive frontend design
- Custom HTTP server written in C
- SQLite integration using the SQLite C API
- JSON communication between the C backend and JavaScript frontend

### Currently Planned / In Development

- Admin Add Room
- Admin Edit Room
- Admin Delete Room
- Department management
- Complete timetable management
- Timetable/routine import
- Floor Manager account management
- Admin authentication
- Floor Manager authentication
- Real college room and routine data
- Improved backend validation
- Production deployment

---

# Problem Statement

In colleges, students often need an empty classroom for activities such as:

- Group discussions
- Project meetings
- Club activities
- Study sessions
- Presentations
- Practice sessions

A timetable can indicate whether a classroom has a scheduled class, but it cannot guarantee that the room is physically empty.

For example:

```text
Room 201

10:00 AM - 11:00 AM
Scheduled Class

11:00 AM - 12:00 PM
No Scheduled Class
```

A timetable-only system would consider Room 201 available after 11:00 AM.

However, students from the previous class may still be using the room.

RoomSync addresses this by maintaining both scheduled and physical occupancy information.

---

# Proposed Solution

RoomSync uses two conditions before displaying a room as available.

## 1. Timetable Availability

The system checks whether any scheduled class overlaps the requested time interval.

## 2. Physical Availability

A Floor Manager can mark each classroom as:

```text
EMPTY
```

or

```text
OCCUPIED
```

A room is displayed to students only when both conditions are satisfied.

```text
Timetable Available
        +
Physical Status = EMPTY
        |
        v
Room Available
```

---

# User Roles

RoomSync is designed around three user roles.

## Student

Students can:

- Select a day
- Select a start time
- Select an end time
- Search for available classrooms
- View room number
- View floor
- View room capacity

Students have read-only access to room availability.

---

## Floor Manager

The Floor Manager dashboard can:

- Retrieve rooms dynamically from SQLite
- Group rooms automatically according to floor
- View room capacity
- View current physical room status
- Mark a room as EMPTY
- Mark a room as OCCUPIED
- View total room count
- View empty room count
- View occupied room count
- Refresh room information from the server

Room information is not hard-coded into the Floor Manager HTML.

---

## Administrator

The Admin interface currently provides:

- Admin dashboard
- Server connection status
- Total room count
- Empty room count
- Occupied room count
- Floor count
- Dynamic room table
- Navigation for future management modules

The following Admin functions are planned but are not yet connected to SQLite:

- Add Room
- Edit Room
- Delete Room
- Manage Departments
- Manage Timetables
- Import Routines
- Manage Floor Managers

---

# Technology Stack

RoomSync is intentionally being developed using fundamental technologies without web frameworks.

| Layer | Technology |
|---|---|
| Frontend Structure | HTML |
| Frontend Styling | CSS |
| Frontend Logic | JavaScript |
| Backend | C |
| Database | SQLite |
| Networking | Windows Winsock |
| Communication | HTTP |
| Data Format | JSON |
| Compiler | GCC / MinGW |

No frontend or backend frameworks are currently used.

---

# System Architecture

```text
                  RoomSync

     +-------------+-------------+
     |             |             |
     v             v             v

  Student     Floor Manager     Admin
     |             |             |
     +-------------+-------------+
                   |
                   | HTTP Requests
                   v
          +------------------+
          |     C Server     |
          |     server.c     |
          |    Port 8080     |
          +--------+---------+
                   |
                   | Function Calls
                   v
          +------------------+
          |    database.c    |
          |   database.h     |
          +--------+---------+
                   |
                   | SQLite C API
                   v
          +------------------+
          |   roomsync.db    |
          |                  |
          |  departments     |
          |  rooms           |
          |  routines        |
          +------------------+
```

The frontend never accesses SQLite directly.

All database operations pass through the C backend.

---

# Project Structure

```text
RoomSync/
|
|-- backend/
|   |-- server.c
|   |-- database.c
|   |-- database.h
|   |-- sqlite3.c
|   |-- sqlite3.h
|   `-- server.exe
|
|-- database/
|   |-- roomsync.db
|   |-- schema.sql
|   `-- seed.sql
|
|-- frontend/
|   |
|   |-- index.html
|   |-- style.css
|   |-- script.js
|   |
|   |-- floor-manager/
|   |   |-- manager.html
|   |   |-- manager.css
|   |   `-- manager.js
|   |
|   `-- admin/
|       |-- admin.html
|       |-- admin.css
|       `-- admin.js
|
|-- .gitignore
`-- README.md
```

---

# Database Design

RoomSync currently uses three main database tables:

```text
departments
rooms
routines
```

## Rooms Table

The `rooms` table stores physical classroom information.

```sql
CREATE TABLE rooms (
    room_id INTEGER PRIMARY KEY AUTOINCREMENT,
    room_number TEXT NOT NULL UNIQUE,
    floor INTEGER NOT NULL,
    capacity INTEGER,
    status TEXT NOT NULL DEFAULT 'EMPTY'
        CHECK(status IN ('EMPTY', 'OCCUPIED')),
    status_updated_at TEXT
);
```

### Important Fields

| Field | Purpose |
|---|---|
| `room_id` | Internal unique room identifier |
| `room_number` | Human-readable classroom number |
| `floor` | Floor on which the room is located |
| `capacity` | Maximum room capacity |
| `status` | Current physical occupancy |
| `status_updated_at` | Time at which occupancy was last updated |

The database restricts room status to:

```text
EMPTY
OCCUPIED
```

---

# Routine Data

Routine records associate scheduled classes with rooms.

Important routine information includes:

```text
Department
Year
Section
Day
Start Time
End Time
Room
Subject
```

RoomSync uses the routine information to determine whether a classroom has a scheduled class during a student's requested interval.

---

# Availability Algorithm

The core RoomSync availability rule is:

```text
ROOM AVAILABLE
      =
NO ROUTINE CONFLICT
      AND
STATUS = EMPTY
```

The SQL logic used by the backend is based on:

```sql
SELECT room_number, floor, capacity
FROM rooms
WHERE status = 'EMPTY'
AND room_id NOT IN
(
    SELECT room_id
    FROM routines
    WHERE day = ?
    AND start_time < ?
    AND end_time > ?
)
ORDER BY floor, room_number;
```

---

# Time Overlap Detection

RoomSync determines whether two time intervals overlap using:

```text
existing_start < requested_end
                AND
existing_end > requested_start
```

For example:

```text
Existing Class:
10:00 -------- 11:00

Requested:
       10:30 -------- 11:30
```

The two intervals overlap because:

```text
10:00 < 11:30
11:00 > 10:30
```

Therefore that room is excluded from the available-room results.

This approach also detects partial and complete interval overlaps.

---

# Backend

The RoomSync backend is written in C.

The backend has two main components:

```text
server.c
database.c
```

## `server.c`

Responsible for:

- Starting the HTTP server
- Creating sockets
- Listening on port 8080
- Receiving browser requests
- Identifying API routes
- Reading URL query parameters
- Calling database functions
- Returning HTTP responses

## `database.c`

Responsible for:

- Opening the SQLite database
- Closing the database
- Executing SQL queries
- Retrieving rooms
- Searching available rooms
- Updating room occupancy
- Converting database results into JSON

## `database.h`

Contains declarations for database functions used by the server.

---

# HTTP Server

RoomSync includes a basic HTTP server written directly in C using Windows Winsock.

The server currently runs at:

```text
http://localhost:8080
```

The basic request lifecycle is:

```text
Create Socket
     |
     v
Bind to Port 8080
     |
     v
Listen
     |
     v
Accept Client
     |
     v
Receive HTTP Request
     |
     v
Process Request
     |
     v
Query / Update SQLite
     |
     v
Send HTTP Response
```

---

# Current API Endpoints

## Test Server

```http
GET /test
```

Used during development to verify communication with the C HTTP server.

---

## Get All Rooms

```http
GET /rooms
```

Returns room information including:

- Room number
- Floor
- Capacity
- Physical status

Example response:

```json
[
    {
        "room_number": "101",
        "floor": 1,
        "capacity": 60,
        "status": "EMPTY"
    }
]
```

---

## Find Available Rooms

```http
GET /available-rooms?day=Monday&start=10:00&end=11:00
```

The backend:

1. Reads the requested day.
2. Reads the requested start time.
3. Reads the requested end time.
4. Finds timetable conflicts.
5. Removes physically occupied rooms.
6. Returns remaining rooms as JSON.

Example response:

```json
[
    {
        "room_number": "101",
        "floor": 1,
        "capacity": 60
    },
    {
        "room_number": "103",
        "floor": 1,
        "capacity": 40
    }
]
```

---

## Update Room Physical Status

```http
GET /room-status?room=101&status=OCCUPIED
```

This endpoint updates the physical room status in SQLite.

The database operation is based on:

```sql
UPDATE rooms
SET status = ?,
    status_updated_at = datetime('now')
WHERE room_number = ?;
```

The implementation uses a prepared SQLite statement and bound parameters.

> Note: The current project uses a GET request for status modification to keep the custom C HTTP implementation simple. A production REST API would normally use a method such as `PATCH` or `PUT` for an update operation.

---

# JSON Communication

The C backend sends structured data to JavaScript using JSON.

Example:

```json
{
    "room_number": "101",
    "floor": 1,
    "capacity": 60,
    "status": "EMPTY"
}
```

JavaScript can then access the values using:

```javascript
room.room_number
room.floor
room.capacity
room.status
```

This provides a structured communication format between the C backend and browser frontend.

---

# Student Search Workflow

When a student searches for a room:

```text
Student enters:
Day + Start Time + End Time
            |
            v
JavaScript validates input
            |
            v
JavaScript creates HTTP request
            |
            v
GET /available-rooms
            |
            v
C server receives request
            |
            v
Query parameters are extracted
            |
            v
database.c queries SQLite
            |
            v
Timetable conflicts are detected
            |
            v
OCCUPIED rooms are filtered
            |
            v
Matching rooms are converted to JSON
            |
            v
HTTP response sent to browser
            |
            v
JavaScript parses JSON
            |
            v
Room cards generated dynamically
```

---

# Floor Manager Workflow

When a Floor Manager changes the status of a room:

```text
Floor Manager
      |
      | Clicks OCCUPIED
      v
manager.js
      |
      | HTTP Request
      v
C Server
      |
      v
updateRoomStatus()
      |
      v
SQLite UPDATE
      |
      v
Room status saved permanently
```

The Floor Manager dashboard then reloads the latest room information from SQLite.

This ensures that the displayed status represents the database state rather than only changing the webpage visually.

---

# Shared Database Behaviour

The Student, Floor Manager and Admin interfaces all depend on the same backend and database.

For example:

```text
Floor Manager
      |
Marks Room 103 OCCUPIED
      |
      v
C Backend
      |
      v
SQLite
      |
      v
Room 103 status = OCCUPIED
      |
      v
Student searches again
      |
      v
Room 103 is filtered out
```

The Student frontend and Floor Manager frontend do not communicate directly with each other.

SQLite acts as the persistent shared source of data through the C backend.

---

# Dynamic Floor Manager Dashboard

The Floor Manager interface does not contain hard-coded classroom numbers.

It requests:

```http
GET /rooms
```

and groups the returned rooms according to their `floor` value.

For example, if the database contains:

```text
101 -> Floor 1
102 -> Floor 1
103 -> Floor 1
201 -> Floor 2
202 -> Floor 2
```

JavaScript dynamically generates:

```text
Floor 1
    Room 101
    Room 102
    Room 103

Floor 2
    Room 201
    Room 202
```

Therefore adding additional rooms to the database does not require manually adding each room to the Floor Manager HTML.

---

# Admin Dashboard

The current Admin dashboard reads room data using:

```http
GET /rooms
```

and dynamically calculates:

- Total rooms
- Empty rooms
- Occupied rooms
- Number of floors

The number of floors is calculated using a JavaScript `Set`, which stores unique floor values.

The Admin room table is also dynamically generated using database data.

Administrative database modification is the next development stage.

---

# Persistence

Room status changes are stored in SQLite.

This means changing:

```text
Room 101
EMPTY -> OCCUPIED
```

does not only change the webpage.

The change is written to:

```text
roomsync.db
```

Therefore refreshing the browser still shows the updated status.

This is different from temporary frontend-only state.

---

# Frontend Design

The frontend is written using only:

```text
HTML
CSS
JavaScript
```

No CSS or JavaScript frameworks are used.

The interface includes:

- Responsive layouts
- Dynamic room cards
- Status badges
- Floor grouping
- Loading states
- Error states
- Server connection indicators
- Admin sidebar navigation
- Mobile responsive layouts

CSS media queries are used to adapt the interface to smaller screens.

---

# Error Handling

The frontend uses JavaScript promise error handling:

```javascript
fetch(url)
    .then(...)
    .catch(...);
```

If the C server is unavailable, the frontend displays an error instead of silently failing.

Examples include:

```text
Server Offline
```

and:

```text
Could not connect to RoomSync server.
```

---

# Input Validation

The Student frontend currently checks:

- Day is selected
- Start time is provided
- End time is provided
- End time is later than start time

Example invalid input:

```text
Start: 14:00
End:   11:00
```

is rejected before the request is sent.

Additional server-side validation is planned.

---

# Database Security Approach

SQLite prepared statements are used for database updates.

For example:

```sql
UPDATE rooms
SET status = ?,
    status_updated_at = datetime('now')
WHERE room_number = ?;
```

Values are bound separately rather than directly concatenated into the SQL statement.

This provides a cleaner and safer database interaction pattern.

---

# Building the Backend

The backend can be compiled using GCC / MinGW:

```powershell
gcc backend/server.c backend/database.c backend/sqlite3.c -o backend/server.exe -lws2_32
```

### Explanation

```text
backend/server.c
```

Compiles the custom C HTTP server.

```text
backend/database.c
```

Compiles RoomSync database functions.

```text
backend/sqlite3.c
```

Compiles the SQLite implementation.

```text
-o backend/server.exe
```

Creates the executable.

```text
-lws2_32
```

Links the Windows Winsock networking library.

---

# Running RoomSync

From the RoomSync project directory, run:

```powershell
.\backend\server.exe
```

The backend listens on:

```text
localhost:8080
```

The frontend pages can then communicate with the running server.

---

# Current Development Data

The project currently uses test classroom and routine data while the complete college dataset is being prepared.

Example test rooms include:

```text
Room 101
Room 102
Room 103
Room 201
Room 202
```

These are development records and are not intended to represent the final college-wide dataset.

---

# Current Limitations

RoomSync is still under development.

Current limitations include:

1. The backend currently runs locally.
2. Authentication has not yet been implemented.
3. Admin CRUD operations are not yet connected.
4. Real college-wide room data has not yet been imported.
5. Real college timetable data has not yet been imported.
6. Physical occupancy is manually maintained by Floor Managers.
7. Backend input validation is currently limited.
8. The custom HTTP implementation is intentionally basic.
9. The current status-update endpoint uses GET rather than a production-style update method.
10. The system has not yet been deployed to a college network or public server.

---

# Planned Development

The next stages of RoomSync include:

```text
Admin Room CRUD
        |
        v
Department Management
        |
        v
Timetable Management
        |
        v
Routine Import
        |
        v
Floor Manager Management
        |
        v
Authentication & Authorization
        |
        v
Real College Dataset
        |
        v
Deployment
```

---

# Future Improvements

Possible future improvements include:

- Secure Admin authentication
- Secure Floor Manager authentication
- Role-based authorization
- Building/block support
- Room type support
- Lab and classroom categorization
- Automated timetable import
- Search by capacity
- Search by building
- Search by room type
- Room status history
- Automatic stale-status handling
- Improved REST API design
- Better HTTP status codes
- Server-side validation
- Network deployment
- Central database deployment
- Optional sensor-based occupancy detection

---

# Key Design Principle

The central idea behind RoomSync is:

```text
A free timetable slot does not necessarily mean
that a classroom is physically free.
```

Therefore:

```text
              ROUTINE DATA
                   |
                   v
           Is a class scheduled?
                   |
                   +
                   |
          PHYSICAL STATUS
                   |
                   v
          Is the room empty?
                   |
                   v

         ACTUAL AVAILABILITY
```

RoomSync attempts to provide a more accurate classroom availability system by combining both sources of information.

---

# Development Approach

RoomSync is being developed incrementally.

The development process followed this order:

```text
SQLite Database
       |
       v
Database Functions in C
       |
       v
Basic Winsock Server
       |
       v
HTTP Endpoints
       |
       v
Student Search
       |
       v
Floor Manager Updates
       |
       v
Dynamic Frontend
       |
       v
Admin Dashboard
       |
       v
Administrative CRUD
       |
       v
Authentication
       |
       v
Real College Data
```

Building the system in layers allows each component to be tested before adding the next layer.

---

# Core Concepts Demonstrated

RoomSync demonstrates several fundamental Computer Science and Software Engineering concepts:

- Relational databases
- SQL queries
- Primary keys
- Foreign keys
- Database constraints
- Prepared statements
- CRUD operations
- C programming
- Pointers
- Character buffers
- Socket programming
- Client-server architecture
- HTTP communication
- API design
- JSON serialization
- JavaScript asynchronous programming
- DOM manipulation
- Event-driven programming
- Input validation
- Persistent storage
- Responsive web design
- Separation of concerns
- Role-based system design

---

# Conclusion

RoomSync is being developed as a lightweight classroom availability platform using fundamental web, networking and database technologies.

The current implementation already demonstrates complete communication between:

```text
Browser Frontend
       |
       v
Custom C HTTP Server
       |
       v
SQLite Database
```

Student searches use both timetable information and physical room status, while Floor Managers can update occupancy information that is persisted in SQLite and subsequently affects Student search results.

The Admin interface establishes the foundation for managing the complete RoomSync dataset. Further development will add administrative CRUD operations, authentication, real college timetable data and deployment support.

---

## RoomSync

**Smart Classroom Availability Management System**

Built using HTML, CSS, JavaScript, C, Winsock and SQLite.
