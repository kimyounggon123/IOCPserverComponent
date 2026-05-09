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
		Task* output = nullptr;
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

			if (!PacketResult::IsValidFlag(pkResult))
			{
				output->packet->set_process_result(PacketResult::Fail);
			}

			/// 데이터 복사 후 기존 거 반환, 이 후 서버로 전송
			Task* ToServer = nullptr;
			if (!dispatcher.BorrowTaskPTR(ToServer, DispatcherID::ProcessToServer)) throw "copy fail";
			ToServer->copyFrom(output);
			if (!dispatcher.EnqueueTaskPTR(std::move(ToServer), DispatcherID::ProcessToServer)) throw "Send()"; // 복사한 데이터를 sender 측으로 전송

			if (pkResult != PacketResult::Fail)
			{
				if (output->broadcastFlag)
				{
					Task* toBroadcast = nullptr;
					if (!dispatcher.BorrowTaskPTR(toBroadcast, DispatcherID::Broadcast)) throw "copy fail";
					toBroadcast->copyFrom(output);
					toBroadcast->packet->set_process_result(PacketResult::Broadcast);
					if (!dispatcher.EnqueueTaskPTR(std::move(toBroadcast), DispatcherID::Broadcast)) throw "Broadcast"; // 복사한 데이터를 sender 측으로 전송
				}
				if (output->DBflag)
				{
					Task* toDB = nullptr;
					if (!dispatcher.BorrowTaskPTR(toDB, DispatcherID::Database)) throw "copy fail";
					toDB->copyFrom(output);
					toDB->packet->set_process_result(PacketResult::WaitDatabase);
					if (!dispatcher.EnqueueTaskPTR(std::move(toDB), DispatcherID::Database)) throw "Database"; // 복사한 데이터를 sender 측으로 전송
				}
			}


			// 기존 사용한 Task 반환
			if (!dispatcher.ReturnTaskPTR(std::move(output), DispatcherID::ServerToProcess)) throw "ReturnTaskPTR()";
		} 
		catch (const char* msg)
		{
			logs.log_error(msg, "PacketProcessThreadPool::work()");
		}
	}

	packetProcess->CloseThreadLocal();
	return 1;
}
