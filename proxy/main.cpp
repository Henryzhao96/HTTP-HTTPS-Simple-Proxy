#include <iostream>
#include <fstream>
#include <string>
#include <process.h>

#include "database.h"
#include "proxy.h"
#include "basicfunctions.h"

using namespace std;

int main()
{
	string cmd = "";
	databaseInit();
	_beginthreadex(NULL, 0, serverInit, NULL, 0, NULL);

	while (getline(cin,cmd)) 
	{
		databaseClose();
		serverShutdown();
		break;
	}
	getchar();
	return 0;
}