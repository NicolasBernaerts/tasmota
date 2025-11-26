/*
  xdrv_98_24_teleinfo_sqlite.ino - SQLite database for Teleinfo ESP32

  Copyright (C) 2025 Nicolas Bernaerts & Olive

  This module implements embedded SQLite database:
  - Power data storage (W, VA, V, A, cosφ)
  - Historical data with timestamps
  - SQL queries for analytics
  - Automatic data rotation
  - REST API for queries

  Benefits:
  - Complex SQL queries (AVG, MAX, GROUP BY)
  - Better performance than CSV
  - Data integrity (ACID transactions)
  - Compact storage with compression
  - Standard SQL interface

  Version history:
    26/11/2025 - v1.0 - Creation for ESP32 POE analytics

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_SQLITE
#ifdef ESP32

#include <sqlite3.h>
#include <FS.h>

/*************************************************\
 *               Configuration
\*************************************************/

#define TIC_SQLITE_DB_PATH       "/littlefs/teleinfo.db"
#define TIC_SQLITE_MAX_RECORDS   100000  // ~100k records = ~2 months at 1 record/minute
#define TIC_SQLITE_CLEANUP_DAYS  90      // Keep 90 days of data

/*************************************************\
 *               Global Variables
\*************************************************/

sqlite3 *teleinfo_db = NULL;

struct {
  bool initialized;
  uint32_t total_records;
  uint32_t insert_count;
  uint32_t query_count;
  uint32_t error_count;
  uint32_t last_insert_ts;
  uint32_t db_size_kb;
} teleinfo_sqlite_stats;

/*************************************************\
 *               Database Schema
\*************************************************/

static const char TIC_SQL_CREATE_TABLES[] PROGMEM = R"(
CREATE TABLE IF NOT EXISTS power_data (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp INTEGER NOT NULL,
  power_w INTEGER,
  power_va INTEGER,
  power_prod_w INTEGER,
  voltage1 INTEGER,
  voltage2 INTEGER,
  voltage3 INTEGER,
  current1 INTEGER,
  current2 INTEGER,
  current3 INTEGER,
  cosphi INTEGER,
  period TEXT,
  contract_level INTEGER
);

CREATE INDEX IF NOT EXISTS idx_timestamp ON power_data(timestamp);
CREATE INDEX IF NOT EXISTS idx_period ON power_data(period);
CREATE INDEX IF NOT EXISTS idx_power_w ON power_data(power_w);

CREATE TABLE IF NOT EXISTS energy_totals (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp INTEGER NOT NULL,
  date TEXT NOT NULL,
  period TEXT,
  total_wh INTEGER,
  total_prod_wh INTEGER
);

CREATE INDEX IF NOT EXISTS idx_energy_date ON energy_totals(date);
CREATE INDEX IF NOT EXISTS idx_energy_period ON energy_totals(period);
)";

/*************************************************\
 *            Database Operations
\*************************************************/

bool TeleinfoSQLiteOpen() {
  int rc;

  if (teleinfo_db != NULL) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("TIC: SQLite already open"));
    return true;
  }

  // Open database
  rc = sqlite3_open(TIC_SQLITE_DB_PATH, &teleinfo_db);

  if (rc != SQLITE_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SQLite open failed: %s"), sqlite3_errmsg(teleinfo_db));
    sqlite3_close(teleinfo_db);
    teleinfo_db = NULL;
    return false;
  }

  // Enable WAL mode for better performance
  char *err_msg = NULL;
  rc = sqlite3_exec(teleinfo_db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    AddLog(LOG_LEVEL_WARNING, PSTR("TIC: SQLite WAL mode failed: %s"), err_msg);
    sqlite3_free(err_msg);
  }

  // Set page size and cache
  sqlite3_exec(teleinfo_db, "PRAGMA page_size=4096;", NULL, NULL, NULL);
  sqlite3_exec(teleinfo_db, "PRAGMA cache_size=2000;", NULL, NULL, NULL);

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite database opened"));

  return true;
}

bool TeleinfoSQLiteCreateTables() {
  char *err_msg = NULL;
  int rc;

  if (!teleinfo_db) return false;

  rc = sqlite3_exec(teleinfo_db, TIC_SQL_CREATE_TABLES, NULL, NULL, &err_msg);

  if (rc != SQLITE_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SQLite create tables failed: %s"), err_msg);
    sqlite3_free(err_msg);
    return false;
  }

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite tables created/verified"));

  return true;
}

void TeleinfoSQLiteClose() {
  if (teleinfo_db) {
    sqlite3_close(teleinfo_db);
    teleinfo_db = NULL;
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite database closed"));
  }
}

/*************************************************\
 *            Insert Power Data
\*************************************************/

bool TeleinfoSQLiteInsertPowerData(
  uint32_t timestamp,
  int power_w,
  int power_va,
  int power_prod_w,
  int voltage1,
  int current1,
  int cosphi,
  const char* period
) {
  sqlite3_stmt *stmt = NULL;
  int rc;

  if (!teleinfo_db) return false;

  const char* sql = "INSERT INTO power_data "
                    "(timestamp, power_w, power_va, power_prod_w, voltage1, current1, cosphi, period) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

  rc = sqlite3_prepare_v2(teleinfo_db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    teleinfo_sqlite_stats.error_count++;
    return false;
  }

  // Bind parameters
  sqlite3_bind_int(stmt, 1, timestamp);
  sqlite3_bind_int(stmt, 2, power_w);
  sqlite3_bind_int(stmt, 3, power_va);
  sqlite3_bind_int(stmt, 4, power_prod_w);
  sqlite3_bind_int(stmt, 5, voltage1);
  sqlite3_bind_int(stmt, 6, current1);
  sqlite3_bind_int(stmt, 7, cosphi);
  sqlite3_bind_text(stmt, 8, period, -1, SQLITE_STATIC);

  // Execute
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    teleinfo_sqlite_stats.error_count++;
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SQLite insert failed: %s"), sqlite3_errmsg(teleinfo_db));
    return false;
  }

  teleinfo_sqlite_stats.insert_count++;
  teleinfo_sqlite_stats.last_insert_ts = timestamp;

  return true;
}

/*************************************************\
 *            Query Functions
\*************************************************/

bool TeleinfoSQLiteQueryStats24h(char* output, size_t len) {
  sqlite3_stmt *stmt = NULL;
  int rc;

  if (!teleinfo_db) {
    strlcpy(output, "{\"error\":\"DB not open\"}", len);
    return false;
  }

  const char* sql = "SELECT "
                    "  AVG(power_w) as avg_w, "
                    "  MAX(power_w) as max_w, "
                    "  MIN(power_w) as min_w, "
                    "  AVG(voltage1) as avg_v, "
                    "  AVG(cosphi) as avg_cosphi, "
                    "  COUNT(*) as count "
                    "FROM power_data "
                    "WHERE timestamp > ?";

  uint32_t cutoff_time = Rtc.utc_time - (24 * 3600);  // 24h ago

  rc = sqlite3_prepare_v2(teleinfo_db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    strlcpy(output, "{\"error\":\"Query failed\"}", len);
    return false;
  }

  sqlite3_bind_int(stmt, 1, cutoff_time);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    int avg_w = sqlite3_column_int(stmt, 0);
    int max_w = sqlite3_column_int(stmt, 1);
    int min_w = sqlite3_column_int(stmt, 2);
    int avg_v = sqlite3_column_int(stmt, 3);
    int avg_cosphi = sqlite3_column_int(stmt, 4);
    int count = sqlite3_column_int(stmt, 5);

    snprintf_P(output, len, PSTR("{\"period\":\"24h\",\"avg_w\":%d,\"max_w\":%d,\"min_w\":%d,\"avg_v\":%d,\"avg_cosphi\":%d,\"samples\":%d}"),
               avg_w, max_w, min_w, avg_v, avg_cosphi, count);
  } else {
    strlcpy(output, "{\"error\":\"No data\"}", len);
  }

  sqlite3_finalize(stmt);
  teleinfo_sqlite_stats.query_count++;

  return true;
}

bool TeleinfoSQLiteQueryHourly(char* output, size_t len, uint8_t hours) {
  sqlite3_stmt *stmt = NULL;
  int rc;
  int offset = 0;

  if (!teleinfo_db || hours > 24) {
    strlcpy(output, "{\"error\":\"Invalid\"}", len);
    return false;
  }

  const char* sql = "SELECT "
                    "  strftime('%H:00', datetime(timestamp, 'unixepoch', 'localtime')) as hour, "
                    "  AVG(power_w) as avg_w, "
                    "  MAX(power_w) as max_w "
                    "FROM power_data "
                    "WHERE timestamp > ? "
                    "GROUP BY hour "
                    "ORDER BY timestamp";

  uint32_t cutoff_time = Rtc.utc_time - (hours * 3600);

  rc = sqlite3_prepare_v2(teleinfo_db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    strlcpy(output, "{\"error\":\"Query failed\"}", len);
    return false;
  }

  sqlite3_bind_int(stmt, 1, cutoff_time);

  // Build JSON array
  offset += snprintf_P(output + offset, len - offset, PSTR("{\"hourly\":["));

  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* hour = (const char*)sqlite3_column_text(stmt, 0);
    int avg_w = sqlite3_column_int(stmt, 1);
    int max_w = sqlite3_column_int(stmt, 2);

    if (!first) offset += snprintf_P(output + offset, len - offset, PSTR(","));
    offset += snprintf_P(output + offset, len - offset, PSTR("{\"h\":\"%s\",\"avg\":%d,\"max\":%d}"),
                        hour, avg_w, max_w);
    first = false;
  }

  offset += snprintf_P(output + offset, len - offset, PSTR("]}"));

  sqlite3_finalize(stmt);
  teleinfo_sqlite_stats.query_count++;

  return true;
}

/*************************************************\
 *            Maintenance
\*************************************************/

uint32_t TeleinfoSQLiteGetRecordCount() {
  sqlite3_stmt *stmt = NULL;
  int rc;
  uint32_t count = 0;

  if (!teleinfo_db) return 0;

  const char* sql = "SELECT COUNT(*) FROM power_data";

  rc = sqlite3_prepare_v2(teleinfo_db, sql, -1, &stmt, NULL);
  if (rc == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  teleinfo_sqlite_stats.total_records = count;

  return count;
}

void TeleinfoSQLiteCleanupOldData() {
  char *err_msg = NULL;
  int rc;
  char sql[128];

  if (!teleinfo_db) return;

  uint32_t cutoff_time = Rtc.utc_time - (TIC_SQLITE_CLEANUP_DAYS * 24 * 3600);

  snprintf_P(sql, sizeof(sql), PSTR("DELETE FROM power_data WHERE timestamp < %u"), cutoff_time);

  rc = sqlite3_exec(teleinfo_db, sql, NULL, NULL, &err_msg);

  if (rc == SQLITE_OK) {
    int deleted = sqlite3_changes(teleinfo_db);
    if (deleted > 0) {
      AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite cleanup: %d old records deleted"), deleted);

      // Vacuum to reclaim space
      sqlite3_exec(teleinfo_db, "VACUUM;", NULL, NULL, NULL);
    }
  } else {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SQLite cleanup failed: %s"), err_msg);
    sqlite3_free(err_msg);
  }
}

uint32_t TeleinfoSQLiteGetDatabaseSize() {
  File file;
  uint32_t size_kb = 0;

#ifdef USE_UFILESYS
  if (ffsp->exists(TIC_SQLITE_DB_PATH)) {
    file = ffsp->open(TIC_SQLITE_DB_PATH, "r");
    if (file) {
      size_kb = file.size() / 1024;
      file.close();
    }
  }
#endif

  teleinfo_sqlite_stats.db_size_kb = size_kb;

  return size_kb;
}

/*************************************************\
 *            Periodic Data Logging
\*************************************************/

void TeleinfoSQLiteLogCurrentData() {
  // Log current power data to database
  // This should be called periodically (e.g., every minute)

  if (!teleinfo_db) return;

#ifdef USE_TELEINFO
  // Get current data from teleinfo structures
  uint32_t timestamp = Rtc.utc_time;
  int power_w = teleinfo_conso.pact;
  int power_va = teleinfo_conso.papp;
  int power_prod_w = 0;  // TODO: Get from production if available
  int voltage = teleinfo_conso.phase[0].voltage;
  int current = teleinfo_conso.phase[0].current;
  int cosphi = teleinfo_conso.cosphi.value;
  char period[16];

  strlcpy(period, teleinfo_contract.str_period, sizeof(period));

  // Insert into database
  TeleinfoSQLiteInsertPowerData(
    timestamp,
    power_w,
    power_va,
    power_prod_w,
    voltage,
    current,
    cosphi,
    period
  );
#endif
}

/*************************************************\
 *               Commands
\*************************************************/

bool TeleinfoSQLiteCommand() {
  bool serviced = false;
  char command[CMND_SIZE];
  char result[512];

  serviced = (XdrvMailbox.data_len > 0);
  if (serviced) {
    strlcpy(command, XdrvMailbox.data, sizeof(command));

    if (strcasecmp(command, "stats") == 0) {
      AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite Statistics:"));
      AddLog(LOG_LEVEL_INFO, PSTR("  DB Size:    %u KB"), TeleinfoSQLiteGetDatabaseSize());
      AddLog(LOG_LEVEL_INFO, PSTR("  Records:    %u"), TeleinfoSQLiteGetRecordCount());
      AddLog(LOG_LEVEL_INFO, PSTR("  Inserts:    %u"), teleinfo_sqlite_stats.insert_count);
      AddLog(LOG_LEVEL_INFO, PSTR("  Queries:    %u"), teleinfo_sqlite_stats.query_count);
      AddLog(LOG_LEVEL_INFO, PSTR("  Errors:     %u"), teleinfo_sqlite_stats.error_count);
    } else if (strcasecmp(command, "query24h") == 0) {
      if (TeleinfoSQLiteQueryStats24h(result, sizeof(result))) {
        Response_P(result);
      }
    } else if (strncasecmp(command, "hourly ", 7) == 0) {
      int hours = atoi(command + 7);
      if (hours > 0 && hours <= 24) {
        if (TeleinfoSQLiteQueryHourly(result, sizeof(result), hours)) {
          Response_P(result);
        }
      }
    } else if (strcasecmp(command, "cleanup") == 0) {
      TeleinfoSQLiteCleanupOldData();
      AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite cleanup completed"));
    } else if (strcasecmp(command, "vacuum") == 0) {
      sqlite3_exec(teleinfo_db, "VACUUM;", NULL, NULL, NULL);
      AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite vacuum completed"));
    } else {
      serviced = false;
    }
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite commands:"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sql stats      = show statistics"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sql query24h   = query last 24h stats"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sql hourly X   = hourly data (1-24h)"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sql cleanup    = cleanup old data"));
    AddLog(LOG_LEVEL_INFO, PSTR("  tic_sql vacuum     = optimize database"));
    serviced = true;
  }

  return serviced;
}

/*************************************************\
 *               Initialization
\*************************************************/

void TeleinfoSQLiteInit() {
  // Initialize statistics
  memset(&teleinfo_sqlite_stats, 0, sizeof(teleinfo_sqlite_stats));

  // Open database
  if (!TeleinfoSQLiteOpen()) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SQLite initialization failed"));
    return;
  }

  // Create tables
  if (!TeleinfoSQLiteCreateTables()) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SQLite table creation failed"));
    return;
  }

  // Get initial record count
  TeleinfoSQLiteGetRecordCount();

  // Get database size
  TeleinfoSQLiteGetDatabaseSize();

  teleinfo_sqlite_stats.initialized = true;

  AddLog(LOG_LEVEL_INFO, PSTR("TIC: SQLite initialized (%u records, %u KB)"),
         teleinfo_sqlite_stats.total_records,
         teleinfo_sqlite_stats.db_size_kb);
}

/*************************************************\
 *               Interface
\*************************************************/

bool Xdrv98_24(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      TeleinfoSQLiteInit();
      break;

    case FUNC_EVERY_MINUTE:
      // Log current data every minute
      TeleinfoSQLiteLogCurrentData();
      break;

    case FUNC_EVERY_HOUR:
      // Cleanup old data every hour
      if (teleinfo_sqlite_stats.total_records > TIC_SQLITE_MAX_RECORDS) {
        TeleinfoSQLiteCleanupOldData();
      }
      break;

    case FUNC_COMMAND:
      if (strcasecmp(XdrvMailbox.topic, "tic_sql") == 0) {
        result = TeleinfoSQLiteCommand();
      }
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoSQLiteClose();
      break;
  }

  return result;
}

#endif  // ESP32
#endif  // USE_TELEINFO_SQLITE
#endif  // USE_TELEINFO
#endif  // USE_ENERGY_SENSOR
