#include "PacketID.h"
int PacketType::count = 0;
const int32_t PacketType::Default =  PacketType::Next();
const int32_t PacketType::ServerIsClosed = PacketType::Next();

int PacketResult::count = 0;
const int32_t PacketResult::Try = PacketResult::Next();
const int32_t PacketResult::Success = PacketResult::Next();
const int32_t PacketResult::Fail = PacketResult::Next();
const int32_t PacketResult::BroadCast = PacketResult::Next();
const int32_t PacketResult::WaitDatabase = PacketResult::Next();