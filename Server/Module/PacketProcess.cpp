#include "PacketProcess.h"
std::unordered_map<PacketProcessKey, HandlerFunc, PacketProcessKeyHash> PacketProcess::func_map;



bool PacketProcess::BroadcastThisInRoom(Task& task, int roomID)
{
	Room* room = roomManager.GetRoom(0);
	if (room == nullptr) return false;

	task.target.type = TARGET_TYPE::Room;
	task.target.room = room;
	task.broadcastFlag = true;
	return true;
}
bool PacketProcess::DBThis(Task& task)
{
	task.DBflag = true;
	return true;
}

std::string PacketProcess::hash_function(const char* key, const char* to_hash) {
	if (!key || !to_hash) return "";
	unsigned long long hash_value = 5381;

	for (size_t i = 0; i < strlen(key); i++) {
		hash_value = ((hash_value << 5) + hash_value) ^ key[i]; // 비트 단위로 xor 연산
	}

	for (size_t i = 0; i < strlen(to_hash); i++) {
		hash_value = ((hash_value << 5) + hash_value) ^ to_hash[i];
		hash_value = (hash_value << 13) | (hash_value >> 19);
		hash_value *= 0x9E3779B9; // 큰 소수로 곱하여 값 퍼뜨리기

	}

	// 최종 해시값을 16진수 문자열로 변환
	std::string hash_result(32, '0');
	for (int i = 0; i < 16; i++) {
		unsigned char byte = (hash_value >> (i * 2)) & 0xFF;
		hash_result[i * 2] = (byte >> 4) < 10 ? '0' + (byte >> 4) : 'a' + ((byte >> 4) - 10);
		hash_result[i * 2 + 1] = (byte & 0x0F) < 10 ? '0' + (byte & 0x0F) : 'a' + ((byte & 0x0F) - 10);
	}

	return hash_result;
}

void PacketProcess::initialize()
{
	func_map.emplace(
		PacketProcessKey{ PacketType::Default, PacketResult::Try},
		[this](Task& input) {return testPacketFunc(input); }
	);
	func_map.emplace(
		PacketProcessKey{ PacketType::ServerIsClosed, PacketResult::Try },
		[this](Task& input) {return closedServerLogic(input); }
	);
}




bool PacketProcess::testPacketFunc(Task& input)
{
	volatile double result = 0; // 부하를 위한 컴파일러 최적화 최소화
	for (int i = 0; i < 10000; ++i) // 
		result += sqrt(i * 1.23);

	input.packet->set_process_result(PacketResult::Success);
	return true;
}

bool PacketProcess::closedServerLogic(Task& input)
{
	logs.log("Server is closed.");
	input.packet->set_process_result(PacketResult::Fail);
	return true;
}

HandlerFunc PacketProcess::getFunc(const Task& input)
{
	PacketProcessKey key{ input.packet->get_type(), input.packet->get_process_result()};
	auto func = func_map.find(key);
	if (func != func_map.end()) return func->second;

	return [this](Task& input)
		{
			printf("Type: (%d), Result: (%d)", input.packet->get_type(), input.packet->get_process_result());
			input.packet->set_process_result(PacketResult::Fail);
			return logs.log_error("type error", "Packet process");
		};
}