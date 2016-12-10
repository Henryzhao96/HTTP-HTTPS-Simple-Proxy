// function.cpp
// 2016-12-08 04:13:17
// Henryzhao

#include <iostream>
#include <fstream>
#include <string>
#include <process.h>

#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include "base64.h"
#include "database.h"
#include "basicfunctions.h"

using namespace std;

#define LISTEN_PORT 12222
#define BUFFER_SIZE 10240

HANDLE socketMutex; //socket R/W lock
bool stopServerSignal = false;
bool proxyAuth = true;
//char* notice = "<script>alert('Welcome to Henryzhao's Proxy Server');</script>";

int isNotEnded(string & data, int & recvedLen, int & type);



string getValueFromHeader(const string & header, const char * key_char)
{
	string key(key_char);
	if (string::npos == header.find(key))
		return "";
	return header.substr((header.find(key) + key.length() + 2), (header.find("\r\n", header.find(key)) - (header.find(key) + key.length() + 2)));
}

int myClose(SOCKET & currentSocket)
{
	shutdown(currentSocket, 2);
	closesocket(currentSocket);
	return 0;
}

int mySend(SOCKET & currentSocket, string & data)
{
	int sendLen = send(currentSocket, data.c_str(), data.length(), 0);
	//logReporting("sending", GetCurrentThreadId());
	if (sendLen <= 0) {
		throw runtime_error("Send Error");
	}
	else return sendLen;
}

int myRecv(SOCKET & currentSocket, string & data)
{
	char buffer[BUFFER_SIZE];
	int recvLen = 0, totalRecvLen = 0, expectedTotalLength = 0;
	int contentType = 0; // 1 for nothing marked, only header; 2 for "Content-Length: "; 3 for "chunked".
	data = "";

	do 
	{
		//logReporting("recving", GetCurrentThreadId());
		//Clear Buffer
		SecureZeroMemory((PVOID)buffer, BUFFER_SIZE);

		recvLen = recv(currentSocket, buffer, BUFFER_SIZE - 1, 0);
		if (recvLen < 0) 
		{
			throw runtime_error("Receive Error");
			return recvLen;
		}
		if (recvLen == 0)
		{
			throw runtime_error("Socket Closed");
			return recvLen;

		}
		data.append(buffer, recvLen);
		totalRecvLen += recvLen;
	} while (isNotEnded(data, totalRecvLen, contentType));
	//cout << data;
	return totalRecvLen;
}

int sslProxy(SOCKET & srcSocket, SOCKET & destSocket)
{
	fd_set fdset;
	int maxPl, ret = 0;
	timeval timeout;
	char sslBuff[BUFFER_SIZE];

	logReporting("WOW! Such HTTPS!", GetCurrentThreadId());
	string info = "HTTP/1.1 200 Connection established\r\n\r\n";
	mySend(srcSocket, info); // Return greeting header.

	timeout.tv_sec = 300;
	timeout.tv_usec = 0;

	maxPl = ((srcSocket >= destSocket) ? (srcSocket) : (destSocket)) + 1;
	while (!stopServerSignal)
	{
		FD_ZERO(&fdset);
		FD_SET(srcSocket, &fdset);
		FD_SET(destSocket, &fdset);
		ret = select(maxPl, &fdset, (fd_set*)0, (fd_set*)0, &timeout);
		if (ret == 0) return 1;
		else if (FD_ISSET(srcSocket, &fdset))
		{
			ret = recv(srcSocket, sslBuff, sizeof(sslBuff), 0);
			if (ret == 0) return 1;
			ret = send(destSocket, sslBuff, ret, 0);
			if (ret == 0) return 1;
		}
		else if (FD_ISSET(destSocket, &fdset))
		{
			ret = recv(destSocket, sslBuff, sizeof(sslBuff), 0);
			if (ret == 0) return 1;
			ret = send(srcSocket, sslBuff, ret, 0);
			if (ret == 0) return 1;
		}
	}
	return 0;
}

int isNotEnded(string & data, int & recvedLen, int & type) // 1 for nothing marked, only header; 2 for "Content-Length: "; 3 for "chunked".
{
	bool status = true; // 0 for ended, 1 for not ended
	int expectedLength = 0;
	if (type == 0 && string::npos != data.find("\r\n\r\n"))
	{
		if (string::npos != data.find("Content-Length: ")) // 2
			type = 2;
		else if (string::npos != data.find("Transfer-Encoding: chunked")) // 3
			type = 3;
		else
			type = 1;
	}
	switch (type)
	{
	case 1:
		if (string::npos != data.find("\r\n\r\n"))
			status = false;
		break;
	case 2:
		expectedLength = data.find("\r\n\r\n") + 4 + stoi(getValueFromHeader(data, "Content-Length"));
		//cout << "expected length: "<<expectedLength<<endl; 
		if (expectedLength <= recvedLen)
			status = false;
		break;
	case 3:
		if (string::npos != data.find("\r\n0\r\n\r\n"))
			status = false;
		break;
	default:
		break;
	}
	return status;
}

void getHostnPort(string & header, string & host, string & port)
{
	try {
		string hostAndPort = getValueFromHeader(header,"Host");
		errorReporting(hostAndPort.c_str(), GetCurrentThreadId());
		if (string::npos != hostAndPort.find(":"))
		{
			host = hostAndPort.substr(0, hostAndPort.find(":"));
			port = hostAndPort.substr(hostAndPort.find(":") + 1);
		}
		else
		{
			host = hostAndPort;
			port = "80";
		}
	}
	catch (const std::exception& e) {
		errorReporting("Error phrasing Host", GetCurrentThreadId());
		cout << e.what();
	}
}

string genErrorPage(int status, char* title, char* extraHeader, char* text)
{
	string data = "";
	char buffer[1024] = {'\0'};
	sprintf_s(buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\nServer: Henryzhao-Honoka-Proxy\r\n%sConnection: close\r\n\r\n", status, title, extraHeader);
	data += buffer;

	sprintf_s(buffer, sizeof(buffer), "\
<!DOCTYPE html>\n\
<html xmlns=\"http://www.w3.org/1999/xhtml\">\n\
<head>\n\
<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n\
<title>%d %s</title>\n\
</head>\n\
<body bgcolor=\"white\">\n\
<center><h1>%d %s</h1></center>\n\
<center>%s<center>\n\
<hr><center>Henryzhao-Honoka-Proxy</center>\n\
</body>\n\
</html>\n", status, title, status, title, text);
	
	data += buffer;
	return data;
}

unsigned int __stdcall newSocketHandler(void* argScoket)
{
	SOCKET srcSocket = *(SOCKET *)argScoket, destSocket;
	ReleaseMutex(socketMutex); // Make main thread to accept new connection
	SOCKADDR_IN destAddr;
	string srcData = "", authData = "";
	string destData = "", destHost = "", destPort = "";
	string request, reqdata, respdata, cachedata;
	
	// Recieve HTTP Request
	try
	{
		myRecv(srcSocket, srcData);
	}
	catch (const std::exception& e)
	{
		errorReporting(e.what(), 0);
		myClose(srcSocket);
		return 1;
	}

	if (proxyAuth)
	{
		authData = getValueFromHeader(srcData, "Proxy-Authorization");
		if (!dbVerifyIdentity(authData))
		{
			destData = genErrorPage(407, "Proxy Authentication Required", "Proxy-Authenticate: Basic realm=\"Henryzhao Honoka Proxy\"\r\n", "Please contact Henryzhao~");
			mySend(srcSocket, destData);
			myClose(srcSocket);
			return 0;
		}

	}
	
	getHostnPort(srcData, destHost, destPort);

	if (dbCheckBlacklist(destHost))
	{
		destData = genErrorPage(451, "Unavailable For Legal Reasons", "", "RFC 7725.");
		mySend(srcSocket, destData);
		myClose(srcSocket);
		return 0;
	}

	// Init destination Socket
	destSocket = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (destSocket == INVALID_SOCKET) errorReporting("destSocket init failed", WSAGetLastError());

	// Lookup domain IP
	ADDRINFO hints;
	SecureZeroMemory((PVOID)& hints, sizeof(hints));
	hints.ai_flags = AI_ALL;
	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_IPV4;
	ADDRINFO* pResult = NULL;
	if (0 != getaddrinfo((LPCSTR)(destHost.c_str()+'\0'), NULL, &hints, &pResult))
	{
		errorReporting("resolve failed", WSAGetLastError());
		errorReporting(destHost.c_str(), 0);
		myClose(srcSocket);
		return 1;
	}

	// Start connection to destination
	SecureZeroMemory((PVOID)& destAddr, sizeof(destAddr));
	destAddr.sin_family = AF_INET;
	destAddr.sin_addr.S_un.S_addr = ((struct sockaddr_in *)pResult->ai_addr)->sin_addr.S_un.S_addr;
	destAddr.sin_port = htons(atoi(destPort.c_str()));

	// Start connection to destination HTTP server
	logReporting("Try to connect", GetCurrentThreadId());
	if (0 != connect(destSocket, (SOCKADDR *)&destAddr, sizeof(destAddr)))
	{
		errorReporting("Error when connect destination", WSAGetLastError());
		myClose(srcSocket);
		myClose(destSocket);
		return 1;
	}

	if (srcData.find("CONNECT") < 3) //HTTPS!!
	{
		sslProxy(srcSocket, destSocket);
	}
	else
	{
		while (!stopServerSignal)
		{
			try
			{
				request = srcData.substr(0, srcData.find("\r\n"));
				cachedata = dbCacheRead(request);
				if (cachedata.length() <= 1) 
				{
					logReporting("Cache mismatch: ", 0);
					logReporting(request.c_str(), GetCurrentThreadId());
					mySend(destSocket, srcData);
					myRecv(destSocket, destData);
					dbCacheAdd(request, srcData, destData);
				}
				else
				{
					logReporting("Cache match: ", cachedata.length());
					logReporting((request).c_str(), GetCurrentThreadId());
					destData = cachedata;
					//cout << "insert" << destData.find("</body>") <<endl;
					//if(string::npos != destData.find("</body>"))destData.insert(destData.find("</body>"), notice);
				}
				mySend(srcSocket, destData);
				myRecv(srcSocket, srcData);
			}
			catch (const std::exception& e)
			{
				errorReporting(e.what(), 0);
				break;
			}
		}
	}
	myClose(destSocket);
	myClose(srcSocket);
	logReporting("Thread Shutdown", GetCurrentThreadId());
	return 0;
}

unsigned int __stdcall serverInit(void* ignore)
{
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		errorReporting("WSAData init failed", WSAGetLastError());
		return 1;
	}

	HANDLE threadHandle;
	SOCKET listenSocket, currentSocket;
	SOCKADDR_IN listenAddr;
	SecureZeroMemory((PVOID)& listenAddr, sizeof(listenAddr));

	listenSocket = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (listenSocket == INVALID_SOCKET)
	{
		errorReporting("serverSocket init failed", WSAGetLastError());
		return 1;
	}

	listenAddr.sin_family = AF_INET;
	listenAddr.sin_addr.s_addr = INADDR_ANY;
	listenAddr.sin_port = htons(LISTEN_PORT);

	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *)&listenAddr, sizeof(listenAddr)))
	{
		errorReporting("Scoket bind error", WSAGetLastError());
		return 1;
	}

	if (listen(listenSocket, 50))
	{
		errorReporting("Listen error", WSAGetLastError());
		return 1;
	}

	while (!stopServerSignal)
	{
		WaitForSingleObject(socketMutex, INFINITE);
		currentSocket = accept(listenSocket, NULL, NULL);
		threadHandle = (HANDLE) _beginthreadex(NULL, 0, newSocketHandler, &currentSocket, 0, NULL);
		logReporting("Call thread!", GetThreadId(threadHandle));
	}
	shutdown(listenSocket, 2);
	closesocket(listenSocket);
	WSACleanup();

	return 0;
}
int serverShutdown()
{
	stopServerSignal = true;
	return 0;
}