#include "PacketProcess.h"
std::unordered_map<PacketProcessKey, HandlerFunc, PacketProcessKeyHash> PacketProcess::func_map;

void PacketProcess::initialize()
{
	func_map.emplace(
		PacketProcessKey{ PacketType::Default, PacketResult::Try },
		[this](TaskQueueInput* input) {return testPacketFunc(input); }
	);
	func_map.emplace(
		PacketProcessKey{ PacketType::ServerIsClosed, PacketResult::Try },
		[this](TaskQueueInput* input) {return closedServerLogic(input); }
	);
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



bool PacketProcess::testPacketFunc(TaskQueueInput* input)
{
	if (input == nullptr) return logs.log_error("got nullptr");

	volatile double result = 0;
	for (int i = 0; i < 10000; ++i)
		result += sqrt(i * 1.23);

	input->packet->set_process_result(PacketResult::Success);
	//printf("test: %lf (Who: %p)\n", result, input);
	//fflush(stdout);
	return true;
}

bool PacketProcess::closedServerLogic(TaskQueueInput* input)
{
	if (input == nullptr) return logs.log_error("got nullptr");
	//logs.log("Server is closed.");
	input->packet->set_process_result(PacketResult::Fail);
	return true;
}

HandlerFunc PacketProcess::getFunc(TaskQueueInput* input)
{
	PacketProcessKey key{ input->packet->get_type(), input->packet->get_process_result()};
	auto func = func_map.find(key);
	if (func != func_map.end()) return func->second;

	return [this](TaskQueueInput* input)
		{
			input->packet->set_process_result(PacketResult::Fail);
			return logs.log_error("type error", "Packet process");
		};
}