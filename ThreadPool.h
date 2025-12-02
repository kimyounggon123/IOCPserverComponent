#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include "SOCKETINFO.h"
#include <process.h>
#include "ThreadSafeQueue.h"
#include <functional>
#include "Logs.h"


class ThreadPool
{
	HANDLE* threadInPool;
	virtual unsigned int workLoop()
	{ 
		return 0;
	}
protected:
	int poolCapacity;
	std::atomic<bool> exit_flag;
	Logs& logs;

	HANDLE workEvent;
public:
	ThreadPool(int poolCapacity):
		poolCapacity(poolCapacity),  exit_flag(false), threadInPool(nullptr), logs(Logs::getInstance()), workEvent(NULL)
	{}

	virtual ~ThreadPool()
	{
		if (threadInPool != nullptr)
		{
			for (int i = 0; i <poolCapacity; i++)
				CloseHandle(threadInPool[i]);
			delete[] threadInPool;
			threadInPool = nullptr;
		}
		CloseHandle(workEvent);
	}

	virtual bool initialize();
	virtual bool Start();
	virtual void Quit() { exit_flag.store(false); }

	static unsigned int WINAPI workerThread(LPVOID param);
};

#endif