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
	packetProcess->registerThreadLocal();

	while (!exit_flag.load())
	{
		TaskQueueInput* output = nullptr;
		try
		{
			if (!dispatcher.dequeue(output, TaskInformation::PacketProcess)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";

			/// packet process
			auto func = packetProcess->getFunc(output);
			result = func(output);
			//if (!result) throw "Packet process";
		
			// 결과에 따라 다른 큐에 input
			uint32_t pkResult = output->packet->get_process_result();

			if (pkResult != PacketResult::Success && pkResult != PacketResult::Fail)
			{
				output->packet->set_process_result(PacketResult::Fail);
			}
			if (!dispatcher.ProcessToSession(output)) throw "enqueueForSendProcess()";

		} 
		catch (const char* msg)
		{
			if (output && !output->isInvalid())
			{
				output->packet->set_process_result(PacketResult::Fail);
				dispatcher.ProcessToSession(output);
			}
			logs.log_error(msg, "PacketProcessThreadPool::work()");
		}
	}

	packetProcess->closeThreadLocal();
	return 1;
}
