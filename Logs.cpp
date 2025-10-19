#define _CRTDBG_MAP_ALLOC
#include "Logs.h"
Logs* Logs::instance = nullptr;

void Logs::printCurrTime()
{
	std::time_t t = std::time(nullptr);
	std::tm now; // 구조체 선언

	localtime_s(&now, &t);

	char buf[32];
	std::strftime(buf, sizeof(buf), "%y-%m-%d %H:%M:%S", &now);
	std::printf("[%s] ", buf);
}
void Logs::err_exit(LPCTSTR msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}


bool Logs::log(const char* msg, const char* where)
{
	if (!debug_mode) return true;

	EnterCriticalSection(&cs);
	printCurrTime();
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	_tprintf(_T("%hs (Where: %hs)\n"), msg, where);
	LeaveCriticalSection(&cs);
	return true;
}

bool Logs::log_error(const char* msg, const char* where)
{
	if (!debug_mode) return false;

	EnterCriticalSection(&cs);
	printCurrTime();
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12); // 12 : red
	_tprintf(_T("ERROR(%d): %hs (Where : %hs)\n"), WSAGetLastError(), msg, where);
	LeaveCriticalSection(&cs);
	return false;
}

void Logs::showMemoryUsage(const char* where)
{
	PROCESS_MEMORY_COUNTERS pmc;
	HANDLE hProcess = GetCurrentProcess();

	_tprintf(_T("\n---------------------------------<Memory Information>--------------------------\n"));
	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
	{
		printCurrTime();
		_tprintf(_T("WorkingSetSize (physical memory used): %.2f KB(Where : %hs)\n"), pmc.WorkingSetSize / 1024.0, where);
		_tprintf(_T("PagefileUsage (physical memory used): %.2f KB(Where : %hs)\n"), pmc.PagefileUsage / 1024.0, where);
	}
	else
	{
		_tprintf(_T("Printing memory is failed"));
	}
	_tprintf(_T("---------------------------------------------------------------------------------\n"));
}

void Logs::showHeapInfo()
{
	_CrtMemState memState;
	_CrtMemCheckpoint(&memState);

	size_t normalBytes = memState.lSizes[_NORMAL_BLOCK];
	size_t crtBytes = memState.lSizes[_CRT_BLOCK];
	size_t freeBytes = memState.lSizes[_FREE_BLOCK];

	_tprintf(_T("\n--------------------<Heap Information>--------------------\n"));
	printCurrTime();
	_tprintf(_T("[Heap] Normal block bytes : %zu\n"), normalBytes);
	_tprintf(_T("[Heap] CRT block bytes    : %zu\n"), crtBytes);
	_tprintf(_T("[Heap] Free block bytes   : %zu\n"), freeBytes);
	_tprintf(_T("[Heap] Total block count  : %zu\n"), memState.lTotalCount);

	size_t totalBytes = normalBytes + crtBytes;
	if (memState.lTotalCount > 0) {
		double avgBlock = (double)totalBytes / memState.lTotalCount;
		_tprintf(_T("[Heap] Block size average: %.4f bytes\n"), avgBlock);
	}
	_tprintf(_T("------------------------------------------------------------\n"));
}


void Logs::showHeapWalk()
{
	//printCurrTime();
	HANDLE heap = GetProcessHeap();
	PROCESS_HEAP_ENTRY entry = { 0 };
	entry.lpData = NULL;


	size_t totalUsed = 0;
	size_t totalFree = 0;
	size_t usedBlocks = 0;
	size_t freeBlocks = 0;

	while (HeapWalk(heap, &entry)) {
		if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
			totalUsed += entry.cbData;
			usedBlocks++;
		}
		else {
			totalFree += entry.cbData;
			freeBlocks++;
		}
	}

	double avgFreeBlock = (freeBlocks > 0) ? (double)totalFree / freeBlocks : 0.0;
	double fragmentation = 1.0 - (avgFreeBlock / (double)totalFree); // 0~1 사이

	double usedRatio = 0.0;
	if (totalUsed + totalFree > 0)
		usedRatio = (double)totalUsed / (totalUsed + totalFree) * 100.0;


	_tprintf(_T("--------------------<Heap Information>--------------------\n"));
	_tprintf(_T("Heap fragmentation: %.2f%%\n"), fragmentation);
	_tprintf(_T("Used heap: %.2f%% (%llu used / %llu total)\n"),
		usedRatio,
		(unsigned long long)totalUsed,
		(unsigned long long)(totalUsed + totalFree));
	_tprintf(_T("Used blocks: %llu / totalUsed bytes: %llu\n"),
		(unsigned long long)usedBlocks, (unsigned long long)totalUsed);
	_tprintf(_T("Free blocks: %llu / totalFree bytes: %llu\n"),
		(unsigned long long)freeBlocks, (unsigned long long)totalFree);
	_tprintf(_T("----------------------------------------------------------\n"));
}
