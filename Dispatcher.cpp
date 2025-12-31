#include "Dispatcher.h"

bool TaskPool::Initialize()
{
	for (int i = 0; i < 10000; i++)
	{
		TaskQueueInput* task = new TaskQueueInput();
		if (!task) return false;
		if (!taskPool.push(task)) return false;
	}
	return true;
}
bool TaskPool::push(TaskQueueInput*& input)
{
	if (input == nullptr) return false;
	input->Reset();
	return taskPool.push(input);
}
bool TaskPool::pop(TaskQueueInput*& output)
{
	return taskPool.pop(output);
}

bool DispatcherUnit::initialize(int poolCount, DWORD timemsPipe)
{
	taskPool = new TaskPool(INFINITE);
	pipe.setTimems(timemsPipe);

	for (int i = 0; i < poolCount; i++)
	{
		TaskQueueInput* task = new TaskQueueInput();
		if (!task) return false;
		if (!taskPool->push(task)) return false;
	}
	return true;
}
void DispatcherUnit::UndoAll()
{
	TaskQueueInput* output = nullptr;

	while (!pipe.isEmpty())
	{
		if (pipe.dequeue(output))
			taskPool->push(output);
	}
}
bool DispatcherUnit::pushPool(TaskQueueInput*& input)
{
	return taskPool->push(input);
}
bool DispatcherUnit::popPool(TaskQueueInput*& output)
{
	return taskPool->pop(output);
}
bool DispatcherUnit::enqueue(TaskQueueInput*& input)
{
	return pipe.enqueue(input);
}
bool DispatcherUnit::dequeue(TaskQueueInput*& output)
{
	return pipe.dequeue(output);
}


Dispatcher* Dispatcher::instance = nullptr;
bool Dispatcher::initialize()
{
	taskWaiting = new DispatcherUnit();
	taskSend = new DispatcherUnit();

	taskWaiting->initialize();
	taskSend->initialize();
	return true;
}

bool Dispatcher::push(TaskQueueInput*& input, const TaskInformation& where)
{
	bool result = false;
	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->pushPool(input);
		break;

	case TaskInformation::Send:
		result = taskSend->pushPool(input);
		break;

	default:
		break;

	}
	return result;
}
bool Dispatcher::pop(TaskQueueInput*& output, const TaskInformation& where)
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

bool Dispatcher::enqueue(TaskQueueInput*& input, const TaskInformation& where)
{
	bool result = false;
	switch (where)
	{
	case TaskInformation::PacketProcess:
		result = taskWaiting->enqueue(input);
		break;

	case TaskInformation::Send:
		result = taskSend->enqueue(input);
		break;

	default:
		break;

	}
	return result;
}


bool Dispatcher::dequeue(TaskQueueInput*& output, const TaskInformation& where)
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



bool Dispatcher::ProcessToSession(TaskQueueInput*& processResult)
{
	TaskQueueInput* toSession = nullptr;
	if (!pop(toSession, TaskInformation::Send)) return false;
	toSession->copyFrom(*processResult);
	if (!push(processResult, TaskInformation::PacketProcess)) return false;
	if (!enqueue(toSession, TaskInformation::Send)) return false;
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