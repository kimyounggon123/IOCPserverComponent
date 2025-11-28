#ifndef _PACKETID_H
#define _PACKETID_H
#include <cstdint>

// Headers
class PacketType
{
public:
	using Type = int32_t;
	static constexpr Type Default = 0;
	static constexpr Type ServerIsClosed = 1;

};
class PacketResult
{
public:
	using Type = int32_t;
	static constexpr Type Try = 0;
	static constexpr Type WaitDatabase = 1;
	static constexpr Type Success = 2;
	static constexpr Type Fail = 3;
	static constexpr Type BroadCast = 4;
};

#endif