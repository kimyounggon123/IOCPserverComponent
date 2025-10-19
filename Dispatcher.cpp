#include "Dispatcher.h"
Dispatcher* Dispatcher::instance = nullptr;
bool Dispatcher::initialize()
{

	for (int i = 0; i < 1000; i++)
	{
		TaskQueueInput* task = new TaskQueueInput();
		if (!task) return false;
		if (!taskPool.push(task)) return false;
	}
	return true;
}

void Dispatcher::undoAllQueue()
{
	TaskQueueInput* output = nullptr;

	while (!taskProcessWaiting.isEmpty())
	{
		if (taskProcessWaiting.dequeue(output))
			taskPool.push(output);
	}

	while (!taskToSendDB.isEmpty())
	{
		if (taskToSendDB.dequeue(output))
			taskPool.push(output);
	}

	while (!taskToSendClient.isEmpty())
	{
		if (taskToSendClient.dequeue(output))
			taskPool.push(output);
	}

}

bool Dispatcher::push(TaskQueueInput*& input)
{
	if (input == nullptr) return false;
	input->sessionInfo = nullptr;
	return taskPool.push(input);
}
bool Dispatcher::pop(TaskQueueInput*& output)
{
	return taskPool.pop(output);
}


bool Dispatcher::enqueue(TaskQueueInput*& input, const QueueInformation& where)
{
	bool result = false;
	switch (where)
	{
	case QueueInformation::PacketProcess:
		result = taskProcessWaiting.enqueue(input);
		break;

	case QueueInformation::Database:
		result = taskToSendDB.enqueue(input);
		break;

	case QueueInformation::Send:
		result = taskToSendClient.enqueue(input);
		break;
	}
	return result;
}


bool Dispatcher::dequeue(TaskQueueInput*& output, const QueueInformation& where)
{
	bool result = false;

	switch (where)
	{
	case QueueInformation::PacketProcess:
		result = taskProcessWaiting.dequeue(output);
		break;

	case QueueInformation::Database:
		result = taskToSendDB.dequeue(output);
		break;

	case QueueInformation::Send:
		result = taskToSendClient.dequeue(output);
		break;
	}


	return result;
}

bool Dispatcher::isEmpty(const QueueInformation & where)
{
	bool result = false;

	switch (where)
	{
	case QueueInformation::PacketProcess:
		result = taskProcessWaiting.isEmpty();
		break;

	case QueueInformation::Database:
		result = taskToSendDB.isEmpty();
		break;

	case QueueInformation::Send:
		result = taskToSendClient.isEmpty();
		break;
	}
	return result;
}