#include "PacketProcessThreadPool.h"
PacketProcessThreadPool::PacketProcessThreadPool(int poolCapacity, PacketProcess* process) : ThreadPool(poolCapacity),
packetProcess(process), dispatcher(DispatcherHub::getInstance())
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
	packetProcess->RegisterThreadLocal();

	while (!exit_flag.load())
	{
		TaskPTR output = nullptr;
		try
		{
			if (!dispatcher.DequeueTaskPTR(output, DispatcherID::ServerToProcess)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";

			/// packet process
			auto func = packetProcess->getFunc(*output);
			result = func(*output);
		
			// 결과에 따라 다른 큐에 input
			uint32_t pkResult = output->packet->get_process_result();

			if (pkResult != PacketResult::Success && pkResult != PacketResult::Fail)
			{
				output->packet->set_process_result(PacketResult::Fail);
			}


			/// 데이터 복사 후 기존 거 반환, 이 후 서버로 전송
			// 1. 복사
			TaskPTR ToServer = nullptr;
			if (!dispatcher.BorrowTaskPTR(ToServer, DispatcherID::ProcessToServer)) throw "copy fail";
			ToServer.get()->copyFrom(output.get());

			// 2. 기존 사용한 Task 반환
			if (!dispatcher.ReturnTaskPTR(std::move(output), DispatcherID::ServerToProcess)) throw "ReturnTaskPTR()";

			// 3. 이 후 복사한 데이터 서버 측으로 전송
			if (!dispatcher.EnqueueTaskPTR(std::move(ToServer), DispatcherID::ProcessToServer)) throw "ReturnTaskPTR()";

		} 
		catch (const char* msg)
		{
			logs.log_error(msg, "PacketProcessThreadPool::work()");
		}
	}

	packetProcess->CloseThreadLocal();
	return 1;
}
