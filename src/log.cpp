#include "stdafx.h"
#include "log.h"

#define SQLITE_ENABLE_FTS5 1
#define SQLITE_THREADSAFE 1 
#include <sqlite/sqlite3.h> 

// ========================================
// Global variables
// ========================================

sqlite3* g_pDB   = 0;

// ========================================
// Init database and log functionality
// ========================================

void logging::Init() {

    char *zErrMsg = 0;
    int  rc;
    char *sql;

	// Test the threading mode (set at compile-time)
	int threadingMode = sqlite3_threadsafe();

	sqlite3_initialize();

    /* Open database */
    if ( sqlite3_open( common::GetTimeStr("./data/%Y-%m-%d %H-%M-%S.log").c_str(), &g_pDB) ) {
        fprintf(stderr, "[LOG] Could not init log. Can't open database: %s\n", sqlite3_errmsg(g_pDB));
        g_pDB = 0;
        return;
    } 

	/* Create table */
    sql = "CREATE VIRTUAL TABLE LOG USING fts5 (MSG); CREATE TABLE PLOG (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp DATETIME,msg TEXT)";

    rc = sqlite3_exec(g_pDB, sql, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK) {  fprintf(stderr, "[LOG] Could not init log. SQL error: %s\n", zErrMsg); sqlite3_free(zErrMsg); return; }
	
	/* Create indices */
	// rc = sqlite3_exec(g_pDB, "CREATE INDEX idxMSG ON LOG(MSG);", 0, 0, &zErrMsg);
	// if (rc != SQLITE_OK) { fprintf(stderr, "[LOG] Could not init log. SQL error: %s\n", zErrMsg); sqlite3_free(zErrMsg); return; }


	/// TODO: WRITE SETTINGS TO LOG!

}

void logging::Log(const char* msg, ...) {

	char *zErrMsg = 0;
	char buffer[4096];

    va_list args;
    va_start(args, msg);
	vsprintf(buffer, msg, args);
    va_end(args);

	std::string sql = "INSERT INTO LOG VALUES (\"" + 
		common::GetTimestampStr() + "," + std::string(buffer) + "\")";
	
	for (int tryNum = 0; tryNum < 5; tryNum++) {
		if (sqlite3_exec(g_pDB, sql.c_str(), 0, 0, &zErrMsg) == SQLITE_OK) { 
			break; 
		}
	}

	sql = "INSERT INTO PLOG (timestamp, msg) VALUES (" +
		common::GetTimestampStr() + ",\"" + std::string(buffer) + "\")";

	for (int tryNum = 0; tryNum < 5; tryNum++) {
		if (sqlite3_exec(g_pDB, sql.c_str(), 0, 0, &zErrMsg) == SQLITE_OK) {
			break;
		}
	}

	return;
}

std::string logging::QueryToJSON(std::string query, int start) {

	std::string    json  = "{ \"log\": [ ";
	sqlite3_stmt * pStmt = 0 ;
	int            nRow  = 0 ;

	// prepare our query
	sqlite3_prepare_v2(g_pDB, "select MSG from LOG limit 1000000;", -1, &pStmt, 0);

	while ( sqlite3_step(pStmt) == SQLITE_ROW ) {
		
		if (nRow > 0) { json += ","; }

		json += "\"" + std::string(reinterpret_cast<const char*>(
			sqlite3_column_text(pStmt, 0))) + "\"";

		nRow++;
	}

	sqlite3_finalize(pStmt);

	return json + " ] }";
}