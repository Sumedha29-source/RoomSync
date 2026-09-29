PRAGMA foreign_keys = ON;

-- Stores all departments
CREATE TABLE departments (
    department_id INTEGER PRIMARY KEY AUTOINCREMENT,
    department_name TEXT NOT NULL UNIQUE
);

-- Stores all classrooms
CREATE TABLE rooms (
    room_id INTEGER PRIMARY KEY AUTOINCREMENT,
    room_number TEXT NOT NULL UNIQUE,
    floor INTEGER NOT NULL,
    capacity INTEGER,

    -- Physical status reported by the floor manager
    status TEXT NOT NULL DEFAULT 'EMPTY'
        CHECK(status IN ('EMPTY', 'OCCUPIED')),

    status_updated_at TEXT
);

-- Stores the college timetable
CREATE TABLE routines (
    routine_id INTEGER PRIMARY KEY AUTOINCREMENT,

    department_id INTEGER NOT NULL,
    year INTEGER NOT NULL,
    section TEXT NOT NULL,

    day TEXT NOT NULL,

    start_time TEXT NOT NULL,
    end_time TEXT NOT NULL,

    room_id INTEGER NOT NULL,
    subject TEXT NOT NULL,

    FOREIGN KEY (department_id)
        REFERENCES departments(department_id),

    FOREIGN KEY (room_id)
        REFERENCES rooms(room_id)
);