#pragma once
#define DATABASE_RPTID 00032

void errorReporting(const char* message, int errNo);
void logReporting(const char* message, int threadID);
