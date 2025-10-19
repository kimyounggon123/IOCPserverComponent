#ifndef _PACKET_H
#define _PACKET_H


#include "stdafx.h"
#include <cstdint>
#include <cstring>

// Headers
enum class PacketType
{
	Default,
	ServerIsClosed,

	SignUp,
	LogIn,
	LogOut,

	BringMap
};

enum class PacketResult 
{
	Try,
	WaitDatabase,
	Success,
	Fail
};

#pragma pack(push, 1)   // 1바이트 단위로 정렬 시작
struct PacketHeader
{
	// Header informations
	bool needToMarshal;
	int clientID;
	PacketType type;
	PacketResult result;
	int length;

	PacketHeader(bool needToMarshal) : clientID(0), needToMarshal(needToMarshal), type(PacketType::Default), result(PacketResult::Try), length(0)
	{}
	PacketHeader(int clientID, bool needToMarshal, PacketType type, PacketResult result) : clientID(clientID), needToMarshal(needToMarshal), type(type), result(result), length(0)
	{}

	PacketHeader(const PacketHeader& other) :
		clientID(other.clientID), needToMarshal(other.needToMarshal), type(other.type), result(other.result), length(other.length)
	{}

	PacketHeader& operator=(const PacketHeader& other)
	{
		if (this != &other) {
			needToMarshal = other.needToMarshal;
			clientID = other.clientID;
			type = other.type;
			result = other.result;
			length = other.length;
		}
		return *this;
	}
};

class Packet
{
	// Header informations
	PacketHeader header;

	// memory data part
	char data[BUFFERSIZE + 1]; // 해당 containor에 들어가는 integer 등의 value는 직접 endian 처리하도록 할까?
	
	bool is_ascii(const std::string& str);
	bool is_ascii(const char* str);

public:
	static const unsigned int end_mark;
	// static const int maxSize = 1042;

	Packet(bool needToMarshal): header(needToMarshal), data{}
	{}
	Packet(int clientID, bool needToMarshal, PacketType type, PacketResult result): header(clientID, needToMarshal, type, result), data{}
	{}

	Packet(const Packet& other) :
		header(other.header)
	{
		memcpy(data, other.data, BUFFERSIZE + 1);
	}
	~Packet()
	{
		memset(data, 0, BUFFERSIZE + 1);
	}

	Packet& operator=(const Packet& other)
	{
		if (this != &other) {
			header = other.header;
			memcpy(data, other.data, BUFFERSIZE + 1);
		}
		return *this;
	}

	/// <control methods>
	// These methods'll be used when you use or input data in the packet
	void CLEAR_PACKET(bool delete_pk = false); // clear packet data. you can change header

	ERROR_CODE inputData(const void* data, const size_t& data_size, size_t& offset);
	ERROR_CODE inputString(const char* data, size_t& offset);
	ERROR_CODE inputString(const std::string& data, size_t& offset);

	ERROR_CODE copyData(void* dest, const size_t& data_size, size_t& offset);
	ERROR_CODE copyString(char* dest, size_t& offset);
	ERROR_CODE copyString(std::string& dest, size_t& offset);

	/// <Serialize methods>
	ERROR_CODE serialize(char* buffer, int& size);
	ERROR_CODE deserialize(const char* buffer, int recvLength, size_t& offset);

	/// <getter / setter>
	void set_header_type(const PacketType& change) noexcept { header.type = change; }
	const PacketType& get_type() noexcept { return header.type; }
	bool is_header(const PacketType& compare) noexcept { return header.type == compare; }

	void set_process_result(const PacketResult& change) noexcept { header.result = change; }
	const PacketResult& get_process_result() noexcept { return header.result; }

	void setClientID(int id) noexcept { header.clientID = id; }
	int getClientID() noexcept { return header.clientID; }

	int getContentsLength() noexcept { return header.length; }
	int getPacketLength() noexcept { return sizeof(PacketHeader) + header.length; }

	// Debug method
	void print_packet_contents(const char* where);
};
#pragma pack(pop)
#endif