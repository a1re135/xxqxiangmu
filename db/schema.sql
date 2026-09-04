CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);


CREATE TABLE IF NOT EXISTS user (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    phone TEXT NOT NULL UNIQUE,
    nickname TEXT NOT NULL,
    avatar_path TEXT,
    balance REAL NOT NULL DEFAULT 0,
    register_time TEXT NOT NULL,
    status INTEGER NOT NULL DEFAULT 1
);


CREATE TABLE IF NOT EXISTS admin (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL
);


CREATE TABLE IF NOT EXISTS station (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    longitude REAL NOT NULL,
    latitude REAL NOT NULL,
    price REAL NOT NULL
);


CREATE TABLE IF NOT EXISTS charger (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL,
    charger_no TEXT NOT NULL UNIQUE,
    type INTEGER NOT NULL,
    power REAL NOT NULL,
    status INTEGER NOT NULL DEFAULT 0,
    total_count INTEGER NOT NULL DEFAULT 0,
    total_minutes INTEGER NOT NULL DEFAULT 0,

    FOREIGN KEY (station_id)
        REFERENCES station(id)
);


CREATE TABLE IF NOT EXISTS charging_order (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    charger_id INTEGER NOT NULL,

    start_time TEXT,
    end_time TEXT,

    energy REAL NOT NULL DEFAULT 0,
    amount REAL NOT NULL DEFAULT 0,

    status INTEGER NOT NULL DEFAULT 0,

    FOREIGN KEY (user_id)
        REFERENCES user(id),

    FOREIGN KEY (charger_id)
        REFERENCES charger(id)
);


CREATE TABLE IF NOT EXISTS ops_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    operation TEXT NOT NULL,
    created_at TEXT NOT NULL
);


CREATE TABLE IF NOT EXISTS load_prediction (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL,
    generated_time TEXT NOT NULL,
    target_time TEXT NOT NULL,
    predicted_load REAL NOT NULL,
    predicted_free_chargers INTEGER NOT NULL,
    is_peak INTEGER NOT NULL DEFAULT 0,

    FOREIGN KEY (station_id)
        REFERENCES station(id)
);

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
