#pragma once

int databaseInit();
int databaseClose();
int dbExec(std::string & sql);
int dbCacheAdd(std::string & request, std::string & reqdata, std::string & respdata);
std::string dbCacheRead(std::string & request);
bool dbCheckBlacklist(std::string & host);
bool dbVerifyIdentity(std::string & namenpasswd);