#include "Dispatcher.h"

bool DispatcherUnit::initialize(int poolCount, DWORD timemsPipe)
{

	for (int i = 0; i < poolCount; i++)
	{
		std::unique_ptr<Task> task = std::make_unique<Task>();
		if (!task) return false;
		if (!pool.AddElement(std::move(task))) return false;
	}

	pipe.setTimems(timemsPipe);
	return true;
}

void DispatcherUnit::UndoAll()
{
	Task* output = nullptr;

	while (!pipe.isEmpty())
	{
		if (pipe.dequeue(output))
			pushPool(std::move(output));
	}
}

bool DispatcherUnit::pushPool(Task*&& input)
{
	if (input == nullptr) return false;
	input->Reset();
	return pool.Push(std::move(input));
}
bool DispatcherUnit::popPool(Task*& output)
{
	return pool.Pop(output);
}

bool DispatcherUnit::enqueue(Task*&& input)
{
	if (!pipe.enqueue(std::move(input)))
	{
		std::terminate();   // or abort, log+exit
		return false;
	}
	return true;
}
bool DispatcherUnit::dequeue(Task*& output)
{
	return pipe.dequeue(output);
}
/*

Dispatcher* Dispatcher::instance = nullptr;




bool Dispatcher::initialize()
{
	if (isInitialized) return true;

	taskWaiting = new DispatcherUnit();
	taskSend = new DispatcherUnit();

	taskWaiting->initialize();
	taskSend->initialize();

	isInitialized = true;
	return true;
}

bool Dispatcher::push(TaskPTR input, const TaskInformation& where)
{
	bool result = false;
	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->pushPool(std::move(input));
		break;

	case TaskInformation::Send:
		result = taskSend->pushPool(std::move(input));
		break;

	default:
		break;

	}
	return result;
}
bool Dispatcher::pop(TaskPTR& output, const TaskInformation& where)
{
	bool result = false;
	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->popPool(output);
		break;

	case TaskInformation::Send:
		result = taskSend->popPool(output);
		break;

	default:
		break;

	}
	return result;
}

bool Dispatcher::enqueue(TaskPTR input, const TaskInformation& where)
{
	bool result = false;
	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->enqueue(std::move(input));
		break;

	case TaskInformation::Send:
		result = taskSend->enqueue(std::move(input));
		break;

	default:
		break;

	}
	return result;
}


bool Dispatcher::dequeue(TaskPTR& output, const TaskInformation& where)
{
	bool result = false;

	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->dequeue(output);
		break;

	case TaskInformation::Send:
		result = taskSend->dequeue(output);
		break;

	default:
		break;
	}

	return result;
}



bool Dispatcher::ProcessToSession(TaskPTR processResult)
{
	// 1. session task pool에서 task 받아 옴
	TaskPTR toSession = nullptr;
	if (!pop(toSession, TaskInformation::Send)) return false;

	// 2. 정보 복사
	toSession->copyFrom(*processResult);

	// 3. process task는 이제 필요 없으므로 다시 반환
	if (!push(std::move(processResult), TaskInformation::PacketProcess)) return false;
	
	// 4. session task pipe에 전송
	if (!enqueue(std::move(toSession), TaskInformation::Send)) return false;
	return true;
}

bool Dispatcher::isEmpty(const TaskInformation& where)
{
	bool result = false;

	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->isEmpty();
		break;

	case TaskInformation::Send:
		result = taskSend->isEmpty();
		break;
	}
	return result;
}
*/

DispatcherHub* DispatcherHub::instance = nullptr;
bool DispatcherHub::ReturnTaskPTR(Task*&& input, const int32_t id)
{
	auto unit = dispatcherMap.find(id);
	if (unit == dispatcherMap.end()) return false;
	return 	unit->second->pushPool(std::move(input));
}
bool DispatcherHub::BorrowTaskPTR(Task*& output, const int32_t id)
{
	auto unit = dispatcherMap.find(id);
	if (unit == dispatcherMap.end()) return false;
	return 	unit->second->popPool(output);
}

bool DispatcherHub::EnqueueTaskPTR(Task*&& input, const int32_t id)
{
	auto unit = dispatcherMap.find(id);
	if (unit == dispatcherMap.end()) return false;
	return 	unit->second->enqueue(std::move(input));
}
bool DispatcherHub::DequeueTaskPTR(Task*& output, const int32_t id)
{
	auto unit = dispatcherMap.find(id);
	if (unit == dispatcherMap.end()) return false;
	return 	unit->second->dequeue(output);
}