-- Departments
INSERT INTO departments (department_name)
VALUES ('CSE');

INSERT INTO departments (department_name)
VALUES ('CSBS');

INSERT INTO departments (department_name)
VALUES ('ECE');


-- Rooms
INSERT INTO rooms (room_number, floor, capacity)
VALUES ('101', 1, 60);

INSERT INTO rooms (room_number, floor, capacity)
VALUES ('102', 1, 60);

INSERT INTO rooms (room_number, floor, capacity)
VALUES ('103', 1, 40);

INSERT INTO rooms (room_number, floor, capacity)
VALUES ('201', 2, 60);

INSERT INTO rooms (room_number, floor, capacity)
VALUES ('202', 2, 40);

-- Sample Class Routines

-- CSE, Year 1, Section A
INSERT INTO routines
(department_id, year, section, day, start_time, end_time, room_id, subject)
VALUES
(1, 1, 'A', 'Monday', '09:00', '10:00', 1, 'Mathematics');

INSERT INTO routines
(department_id, year, section, day, start_time, end_time, room_id, subject)
VALUES
(1, 1, 'A', 'Monday', '10:00', '11:00', 2, 'Programming');


-- CSBS, Year 1, Section A
INSERT INTO routines
(department_id, year, section, day, start_time, end_time, room_id, subject)
VALUES
(2, 1, 'A', 'Monday', '10:00', '11:00', 4, 'Management');


-- ECE, Year 2, Section A
INSERT INTO routines
(department_id, year, section, day, start_time, end_time, room_id, subject)
VALUES
(3, 2, 'A', 'Monday', '11:00', '12:00', 1, 'Physics');