#include "PacketID.h"
int PacketType::count = 0;
const int32_t PacketType::Default = PacketType::Base + PacketType::Next();
const int32_t PacketType::ServerIsClosed = PacketType::Base + PacketType::Next();

int PacketResult::count = 0;
const int32_t PacketResult::Try = PacketResult::Base + PacketResult::Next();
const int32_t PacketResult::WaitDatabase = PacketResult::Base + PacketResult::Next();
const int32_t PacketResult::Success = PacketResult::Base + PacketResult::Next();
const int32_t PacketResult::Fail = PacketResult::Base + PacketResult::Next();
const int32_t PacketResult::BroadCast = PacketResult::Base + PacketResult::Next();