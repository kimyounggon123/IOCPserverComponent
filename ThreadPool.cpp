#include "ThreadPool.h"


bool ThreadPool::initialize()
{
	workEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	threadInPool = new HANDLE[poolCapacity];
	if (!threadInPool) return false;
	return true;
}

bool ThreadPool::Start()
{
	// make thread pool
	if (!threadInPool) return false;
	for (int i = 0; i < poolCapacity; i++) {
		threadInPool[i] = (HANDLE)_beginthreadex(
			NULL, 0,
			workerThread, this,
			0, NULL);
		if (threadInPool[i] == NULL) return false;
	}

	return true;
}

unsigned int WINAPI ThreadPool::workerThread(LPVOID param)
{
	ThreadPool* This = reinterpret_cast<ThreadPool*>(param);
	DWORD result = 1;

	result = This->workLoop();
	
	return result; 
}
