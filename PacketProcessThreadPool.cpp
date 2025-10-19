#include "PacketProcessThreadPool.h"
PacketProcessThreadPool::PacketProcessThreadPool(int poolCapacity, PacketProcess* process) : ThreadPool(poolCapacity),
packetProcess(process), dispatcher(Dispatcher::getInstance())
{}

bool PacketProcessThreadPool::initialize()
{
	if (!ThreadPool::initialize()) return false;

	if (packetProcess == nullptr || !packetProcess->getInitialized()) return false;

	return true;
}

unsigned int PacketProcessThreadPool::workLoop() // in while loop
{
	DWORD result = 0;
	result = packetProcess->registerThreadLocal();

	while (!exit_flag.load())
	{
		TaskQueueInput* output = nullptr;
		try
		{
			if (!dispatcher.dequeue(output, QueueInformation::PacketProcess)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";

			/// packet process
			auto func = packetProcess->getFunc(output);
			result = func(output);
			if (!result) throw "Packet process";
		
			// 결과에 따라 다른 큐에 input
			PacketResult pkResult = output->packet->get_process_result();
			if (pkResult == PacketResult::WaitDatabase)
			{
				if (!dispatcher.enqueue(output, QueueInformation::Database)) throw "enqueueForDBconnector()";
			}
			else if (pkResult == PacketResult::Success || pkResult == PacketResult::Fail)
			{
				if (!dispatcher.enqueue(output, QueueInformation::Send)) throw "enqueueForSendProcess()";
			}
			else
			{
				output->packet->set_process_result(PacketResult::Fail);
				dispatcher.enqueue(output, QueueInformation::Send);
				throw "wrong packet result!!";
			}
		} 
		catch (const char* msg)
		{
			logs.log_error(msg, "PacketProcessThreadPool::work()");
		}
	}
	return 1;
}
