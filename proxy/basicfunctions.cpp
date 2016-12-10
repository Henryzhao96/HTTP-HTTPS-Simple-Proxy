#include <iostream>
#include <string>

using namespace std;

void errorReporting(const char* message, int errNo)
{
	cout << errNo << ": " << message << endl;
}

void logReporting(const char* message, int threadID)
{
	cout << threadID << ": " << message << endl;
}