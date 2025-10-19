#ifndef _LOGS_H
#define _LOGS_H

#include <Windows.h>
#include <psapi.h>
#include <tchar.h>

#include <crtdbg.h>
#include <chrono>
#include <ctime>

class Logs
{

	bool debug_mode;

	CRITICAL_SECTION cs;
	static Logs* instance;

	Logs(): debug_mode(true)
	{
		InitializeCriticalSection(&cs);
	}


	void printCurrTime();
public:
	static Logs& getInstance()
	{
		if (instance == nullptr) instance = new Logs();
		return *instance;
	}

	~Logs()
	{
		DeleteCriticalSection(&cs);
	}

	void err_exit(LPCTSTR msg); // exit

	bool log(const char* msg, const char* where = "Empty");
	bool log_error(const char* msg, const char* where = "Empty");

	void showMemoryUsage(const char* where = "Empty");
	void showHeapInfo();
	void showCommands();
	void showHeapWalk();
};
using Debug = Logs;
#endif