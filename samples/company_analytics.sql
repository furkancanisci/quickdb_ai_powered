-- Realistic company analytics schema for QuickDb AI end-to-end test.
PRAGMA foreign_keys = ON;

DROP TABLE IF EXISTS reviews;
DROP TABLE IF EXISTS timesheets;
DROP TABLE IF EXISTS projects;
DROP TABLE IF EXISTS employees;
DROP TABLE IF EXISTS departments;

CREATE TABLE departments (
    id      INTEGER PRIMARY KEY,
    name    TEXT    NOT NULL,
    region  TEXT    NOT NULL,
    budget  REAL    NOT NULL
);

CREATE TABLE employees (
    id                 INTEGER PRIMARY KEY,
    name               TEXT    NOT NULL,
    department_id      INTEGER NOT NULL REFERENCES departments(id),
    role               TEXT    NOT NULL,
    hire_date          TEXT    NOT NULL,   -- ISO date
    salary             REAL    NOT NULL,
    performance_score  INTEGER NOT NULL    -- 0..100
);

CREATE TABLE projects (
    id             INTEGER PRIMARY KEY,
    name           TEXT    NOT NULL,
    department_id  INTEGER NOT NULL REFERENCES departments(id),
    start_date     TEXT    NOT NULL,
    end_date       TEXT,
    status         TEXT    NOT NULL,        -- active / done / on_hold
    spend          REAL    NOT NULL
);

CREATE TABLE timesheets (
    id           INTEGER PRIMARY KEY,
    employee_id  INTEGER NOT NULL REFERENCES employees(id),
    project_id   INTEGER NOT NULL REFERENCES projects(id),
    week         TEXT    NOT NULL,          -- ISO monday date
    hours        REAL    NOT NULL
);

CREATE TABLE reviews (
    id           INTEGER PRIMARY KEY,
    employee_id  INTEGER NOT NULL REFERENCES employees(id),
    quarter      TEXT    NOT NULL,          -- e.g. 2025Q1
    rating       INTEGER NOT NULL           -- 1..5
);

INSERT INTO departments (id, name, region, budget) VALUES
 (1, 'Engineering', 'Istanbul',  2400000),
 (2, 'Sales',       'Ankara',     900000),
 (3, 'Marketing',   'Izmir',      600000),
 (4, 'Finance',     'Istanbul',   750000),
 (5, 'Support',     'Bursa',      450000);

INSERT INTO employees (id, name, department_id, role, hire_date, salary, performance_score) VALUES
 (1,  'Ayse Yilmaz',    1, 'Backend Engineer',  '2019-03-11', 145000, 92),
 (2,  'Mehmet Kaya',    1, 'DevOps Engineer',   '2020-07-01', 132000, 88),
 (3,  'Zeynep Demir',   1, 'Tech Lead',         '2016-01-20', 180000, 95),
 (4,  'Can Ozturk',     1, 'Frontend Engineer', '2021-11-05', 118000, 74),
 (5,  'Elif Sahin',     1, 'Data Engineer',     '2022-02-14', 126000, 81),
 (6,  'Burak Celik',    2, 'Account Executive', '2018-09-01',  95000, 79),
 (7,  'Selin Arslan',   2, 'Sales Manager',     '2015-05-18', 140000, 90),
 (8,  'Emre Dogan',     2, 'Account Executive', '2023-01-09',  82000, 61),
 (9,  'Merve Koc',      2, 'Sales Rep',         '2020-03-30',  88000, 68),
 (10, 'Ahmet Aslan',    3, 'Growth Lead',       '2019-06-24', 121000, 86),
 (11, 'Deniz Cetin',    3, 'Content Specialist','2022-08-15',  76000, 72),
 (12, 'Kerem Polat',    3, 'Brand Manager',     '2017-12-01', 109000, 83),
 (13, 'Gizem Erdem',    4, 'Financial Analyst', '2021-04-19', 102000, 85),
 (14, 'Omer Kilic',     4, 'Accountant',        '2014-02-10',  97000, 77),
 (15, 'Pelin Yildiz',   4, 'Controller',        '2018-10-22', 138000, 91),
 (16, 'Tolga Aksoy',    5, 'Support Agent',     '2022-05-02',  64000, 58),
 (17, 'Ebru Candan',    5, 'Support Lead',      '2016-07-11',  92000, 84),
 (18, 'Sina Turk',      5, 'Support Agent',     '2023-06-01',  60000, 55);

INSERT INTO projects (id, name, department_id, start_date, end_date, status, spend) VALUES
 (1, 'Platform Rebuild',  1, '2024-01-08', '2024-12-20', 'done',    520000),
 (2, 'Mobile App',        1, '2025-02-01', NULL,         'active',  310000),
 (3, 'Data Warehouse',    1, '2025-05-15', NULL,         'active',  240000),
 (4, 'CRM Rollout',       2, '2024-03-01', '2024-11-30', 'done',    180000),
 (5, 'Enterprise Deals',  2, '2025-01-10', NULL,         'active',  260000),
 (6, 'Brand Refresh',     3, '2024-06-01', '2024-09-30', 'done',     95000),
 (7, 'Q3 Campaign',       3, '2025-07-01', NULL,         'active',  140000),
 (8, 'Audit Automation',  4, '2025-04-01', NULL,         'on_hold',  60000),
 (9, 'Cost Control',      4, '2024-02-15', '2024-08-15', 'done',     45000),
 (10,'Support Portal',    5, '2025-03-01', NULL,         'active',   70000);

INSERT INTO timesheets (employee_id, project_id, week, hours) VALUES
 (1,1,'2025-01-06',38),(1,2,'2025-01-13',34),(3,1,'2025-01-06',41),(3,3,'2025-01-20',36),
 (2,3,'2025-02-03',33),(5,3,'2025-02-10',40),(4,2,'2025-02-17',29),(6,4,'2024-04-01',35),
 (7,5,'2025-01-27',44),(9,5,'2025-02-24',31),(10,7,'2025-07-07',37),(12,6,'2024-07-01',26),
 (13,9,'2024-03-04',30),(15,8,'2025-04-07',39),(17,10,'2025-03-10',33),(16,10,'2025-03-17',22);

INSERT INTO reviews (employee_id, quarter, rating) VALUES
 (1,'2025Q1',5),(1,'2025Q2',4),(3,'2025Q1',5),(3,'2025Q2',5),(2,'2025Q1',4),
 (5,'2025Q2',4),(4,'2025Q2',3),(7,'2025Q1',5),(6,'2025Q1',4),(8,'2025Q2',2),
 (10,'2025Q2',4),(12,'2025Q1',4),(13,'2025Q2',4),(15,'2025Q1',5),(17,'2025Q2',4),
 (16,'2025Q2',3),(18,'2025Q2',2),(9,'2025Q1',3);

CREATE INDEX idx_emp_dept ON employees(department_id);
CREATE INDEX idx_proj_dept ON projects(department_id);
CREATE INDEX idx_ts_emp ON timesheets(employee_id);
