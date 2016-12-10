#include <iostream>
#include <fstream>
#include <string>

#include "sqlite3.h"
#include "basicfunctions.h"

using namespace std;

sqlite3 *db;

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
	int i;
	string info;
	for (i = 0; i<argc; i++) 
	{
		info.assign(azColName[i]);
		info.append(" = ");
		info.append(argv[i] ? argv[i] : "NULL");
		logReporting(info.c_str(), DATABASE_RPTID);
	}
	return 0;
}

int dbExec(string & sql)
{
	int ret;
	char *zErrMsg = 0;
	ret = sqlite3_exec(db, sql.c_str(), callback, 0, &zErrMsg);
	if (ret != SQLITE_OK) {
		errorReporting(zErrMsg, DATABASE_RPTID);
		sqlite3_free(zErrMsg);
		return 1;
	}
	return 0;
}

int dbCacheAdd(string & request, string & reqdata, string & respdata)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2(db,
		"INSERT INTO history(request, reqdata, respdata)"
		" VALUES(?, ?, ?);",
		-1, &stmt, NULL);

	if (ret != SQLITE_OK) {
		errorReporting(sqlite3_errmsg(db), DATABASE_RPTID);
	}
	else 
	{
		// SQLITE_STATIC because the statement is finalized
		// before the buffer is freed:
		ret = sqlite3_bind_text(stmt, 1, request.c_str(), request.length(), SQLITE_STATIC);
		ret += sqlite3_bind_blob(stmt, 2, reqdata.c_str(), reqdata.length(), SQLITE_STATIC);
		ret += sqlite3_bind_blob(stmt, 3, respdata.c_str(), respdata.length(), SQLITE_STATIC);
		if (ret != SQLITE_OK) {
			errorReporting(sqlite3_errmsg(db), DATABASE_RPTID);
			return 1;
		}
		else 
		{
			ret = sqlite3_step(stmt);
			if (ret != SQLITE_DONE) 
			{
				errorReporting(sqlite3_errmsg(db), DATABASE_RPTID);
				return 1;
			}
		}
		sqlite3_finalize(stmt);
	}
	return 0;
}

string dbCacheRead(string & request)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;
	string result, sql;
	sql.assign("SELECT (respdata) FROM history WHERE request = '");
	sql.append(request);
	sql.append("';");

	//cout << sql << endl;

	ret = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		errorReporting(sqlite3_errmsg(db), 1);
	}
	else
	{
		// SQLITE_STATIC because the statement is finalized
		// before the buffer is freed:
		//ret = sqlite3_bind_text(stmt, 1, request.c_str(), request.length(), SQLITE_STATIC);
		if (ret != SQLITE_OK) {
			errorReporting(sqlite3_errmsg(db), 2);
			return "";
		}

		ret = sqlite3_step(stmt);
		if (ret != SQLITE_ROW)
		{
			errorReporting(sqlite3_errmsg(db), 3);
			return "";
		}
		result.assign((const char*)sqlite3_column_blob(stmt, 0), sqlite3_column_bytes(stmt, 0));
		sqlite3_finalize(stmt);
	}
	return result;
}

bool dbCheckBlacklist(string & host)
{
	int ret = 0;
	bool result = false;
	sqlite3_stmt *stmt = NULL;
	string sql;
	sql.assign("SELECT (host) FROM blacklist WHERE host = '");
	sql.append(host);
	sql.append("';");

	//cout << sql << endl;

	ret = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		errorReporting(sqlite3_errmsg(db), 1);
	}
	else
	{
		// SQLITE_STATIC because the statement is finalized
		// before the buffer is freed:
		if (ret != SQLITE_OK) {
			errorReporting(sqlite3_errmsg(db), 2);
			return false;
		}

		ret = sqlite3_step(stmt);
		if (ret != SQLITE_ROW)
		{
			errorReporting(sqlite3_errmsg(db), 3);
			return false;
		}
		//result.assign((const char*)sqlite3_column_text(stmt, 0), sqlite3_column_bytes(stmt, 0));
		if (sqlite3_column_bytes(stmt, 0) > 0)
			result = true;
		sqlite3_finalize(stmt);
	}
	return result;
}

bool dbVerifyIdentity(string & namenpasswd)
{
	if (namenpasswd.length() <= 0) return false;
	namenpasswd = namenpasswd.substr(6);
	int ret = 0;
	bool result = false;
	sqlite3_stmt *stmt = NULL;
	string sql;
	sql.assign("SELECT (username) FROM user WHERE key = '");
	sql.append(namenpasswd);
	sql.append("';");

	//cout << sql << endl;

	ret = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		errorReporting(sqlite3_errmsg(db), 1);
	}
	else
	{
		// SQLITE_STATIC because the statement is finalized
		// before the buffer is freed:
		if (ret != SQLITE_OK) {
			errorReporting(sqlite3_errmsg(db), 2);
			return false;
		}

		ret = sqlite3_step(stmt);
		if (ret != SQLITE_ROW)
		{
			errorReporting(sqlite3_errmsg(db), 3);
			return false;
		}
		//result.assign((const char*)sqlite3_column_text(stmt, 0), sqlite3_column_bytes(stmt, 0));
		if (sqlite3_column_bytes(stmt, 0) > 0)
			result = true;
		sqlite3_finalize(stmt);
	}
	return result;
}

int databaseInit()
{
	int ret;
	string sql;
	char *zErrMsg = 0;

	ret = sqlite3_open("database.db", &db);
	if (ret)
	{
		errorReporting(sqlite3_errmsg(db), DATABASE_RPTID);
		return 1;
	}
	else 
		logReporting("Opened database successfully", DATABASE_RPTID);

	ret = 0;
	// Create histroy table
	sql = "CREATE TABLE IF NOT EXISTS history("  \
		"request        TEXT    PRIMARY KEY  NOT NULL," \
		"reqip          TEXT    ," \
		"respip         TEXT    ," \
		"reqdata        BLOB    ," \
		"respdata       BLOB    );";

	ret = ret + dbExec(sql);

	// Create blacklist table
	sql = "CREATE TABLE IF NOT EXISTS blacklist("  \
		"host           TEXT    PRIMARY KEY NOT NULL," \
		"times          INT     ," \
		"reason         TEXT    );";

	ret = ret + dbExec(sql);

	// Create user table
	sql = "CREATE TABLE IF NOT EXISTS user("  \
		"username       TEXT    PRIMARY KEY NOT NULL," \
		"password       TEXT    ," \
		"key            TEXT    );";

	ret = ret + dbExec(sql);
	if (ret == 0)
		logReporting("Database created successful!", DATABASE_RPTID);
	else errorReporting("Database created unsuccessful!", DATABASE_RPTID);

	return 0;
}

int databaseClose()
{
	sqlite3_close(db);
	return 0;
}