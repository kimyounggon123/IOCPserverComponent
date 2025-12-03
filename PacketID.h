#ifndef _PACKETID_H
#define _PACKETID_H
#include <cstdint>


// Headers
struct PacketType
{
	static constexpr int32_t Base = 0;
	static constexpr int32_t Default =			Base + 0;
	static constexpr int32_t ServerIsClosed =	Base + 1;
};

struct PacketResult
{
	static constexpr int32_t Base = 0; 

	static constexpr int32_t Try =			Base + 0;
	static constexpr int32_t Success =		Base + 1;
	static constexpr int32_t Fail =			Base + 2;
	static constexpr int32_t BroadCast =	Base + 3;
	static constexpr int32_t WaitDatabase = Base + 4;
};


#endif