#include "Packet.h"
const unsigned int Packet::end_mark = 0xffffffff;
void Packet::CLEAR_PACKET(bool delete_pk) {
	memset(data, 0, BUFFERSIZE + 1);
	header.length = 0;
	if (delete_pk) set_header_type(PacketType::Default);
}
bool Packet::is_ascii(const std::string& str) {
	for (char c : str) {
		// check utf
		if (static_cast<unsigned char>(c) > 127) return false;
	}
	return true;
}
bool Packet::is_ascii(const char* str) {
	int flag = 0;
	while (str[flag] != '\0') {
		// check utf
		if (str[flag] > 127) return false;
		flag++;
	}
	return true;
}

ERROR_CODE Packet::inputData(const void* src, const size_t& data_size, size_t& offset) {
	if (!src) return ERROR_CODE::GET_NULLPTR;
	if (offset + data_size > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	memcpy(data + offset, src, data_size);
	offset += data_size;

	// reset pk length
	header.length = static_cast<int>(offset);

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::inputDataInt(int32_t src, size_t& offset)
{
	if (offset + sizeof(int32_t) > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	int32_t endianData = htonl(src);

	memcpy(data + offset, &endianData, sizeof(int32_t));
	offset += sizeof(int32_t);

	// reset pk length
	header.length = static_cast<int32_t>(offset);

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::inputDataFloat(float src, size_t& offset)
{
	if (offset + sizeof(float) > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	// static_assert 컴파일 시점에서 보여주는 에러 코드 false면 적은 에러 메시지 출력
	static_assert(sizeof(float) == sizeof(uint32_t), "float size must be 4 bytes"); 

	// float → uint32_t (비트 복사)
	uint32_t tmp;
	memcpy(&tmp, &src, sizeof(float));

	// network byte order
	tmp = htonl(tmp);

	// buffer에 복사
	memcpy(data + offset, &tmp, sizeof(float));
	offset += sizeof(float);

	// header length 갱신 (데이터 길이 기준)
	header.length = static_cast<int>(offset);

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::inputString(const char* src, size_t& offset) {
	if (!src) return ERROR_CODE::GET_NULLPTR;

	bool ascii_flag = is_ascii(src);
	int str_len = static_cast<int>(strlen(src));

	// flag(1 byte) + length(4 byte) + string data
	if (offset + 1 + sizeof(int) + str_len > BUFFERSIZE) 
		return ERROR_CODE::INCORRECT_SIZE;

	// store encoding flag
	data[offset++] = ascii_flag ? 0 : 1;

	// store string length
	ERROR_CODE code = inputDataInt(str_len, offset);
	if (!code) return code;

	// copy string data
	memcpy(data + offset, src, str_len);
	offset += str_len;

	header.length = static_cast<int>(offset);
	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::inputString(const std::string& src, size_t& offset) {
	bool ascii_flag = is_ascii(src);
	int str_len = static_cast<int>(src.size());

	if (offset + 1 + sizeof(int) + str_len > BUFFERSIZE)
		return ERROR_CODE::INCORRECT_SIZE;

	data[offset++] = ascii_flag ? 0 : 1;

	// store string length (한 번만)
	ERROR_CODE code = inputDataInt(str_len, offset);
	if (code != ERROR_CODE::SUCCESS) return code;

	memcpy(data + offset, src.data(), str_len);
	offset += str_len;

	header.length = static_cast<int>(offset);
	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::readData(void* dest, const size_t& data_size, size_t& offset) {
	if (!dest) return ERROR_CODE::GET_NULLPTR;
	if (offset + data_size > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	// copy data
	memcpy(dest, data + offset, data_size);
	offset += data_size;

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::readDataInt(int32_t* dest, size_t& offset)
{
	if (!dest) return ERROR_CODE::GET_NULLPTR;
	if (offset + sizeof(int32_t) > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	// copy data
	int32_t temp;
	memcpy(&temp, data + offset, sizeof(int32_t));
	offset += sizeof(int32_t);
	*dest = ntohl(temp);
	
	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::readDataFloat(float* dest, size_t& offset)
{
	if (!dest) return ERROR_CODE::GET_NULLPTR;
	if (offset + sizeof(float) > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	uint32_t tmp;
	memcpy(&tmp, data + offset, sizeof(uint32_t));
	offset += sizeof(float);

	tmp = ntohl(tmp);

	memcpy(dest, &tmp, sizeof(float));

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::readString(char* dest, size_t& offset) {
	if (!dest) return ERROR_CODE::GET_NULLPTR;

	// check string flag
	bool is_utf8 = data[offset++] == 1;

	// read string length
	int32_t str_len;
	ERROR_CODE code = readDataInt(&str_len, offset);
	if (!code) return code;

	// read real string
	if (offset + str_len > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	memcpy(dest, data + offset, str_len);
	offset += str_len;

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::readString(std::string& dest, size_t& offset) {

	// check string flag
	bool is_utf8 = data[offset++] == 1;

	// read string length
	int32_t str_len;
	ERROR_CODE code = readDataInt(&str_len, offset);
	if (!code) return code;

	// read real string
	if (offset + str_len > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	dest.assign(data + offset, str_len);
	offset += str_len;

	return ERROR_CODE::SUCCESS;
}

/// <Serialize methods>
void Packet::inputHeader(char* buffer, size_t& offset)
{
	int32_t endianClientID = htonl(header.clientID);
	int32_t endianType = htonl(static_cast<int32_t>(header.type));
	int32_t endianResult = htonl(static_cast<int32_t>(header.result));
	int32_t endianLength = htonl(header.length);
	
	memcpy(buffer + offset, &endianClientID, sizeof(int32_t));
	offset += sizeof(int32_t);
	memcpy(buffer + offset, &endianType, sizeof(int32_t));
	offset += sizeof(int32_t);
	memcpy(buffer + offset, &endianResult, sizeof(int32_t));
	offset += sizeof(int32_t);
	memcpy(buffer + offset, &endianLength, sizeof(int32_t));
	offset += sizeof(int32_t);
}

ERROR_CODE Packet::serialize(char* buffer) {
	// parameter buffer size를 체크해야 함
	if (!buffer) return ERROR_CODE::GET_NULLPTR;

	// check size
	if (header.length < 0 || sizeof(PacketHeader) + header.length + sizeof(int) > BUFFERSIZE)
		return ERROR_CODE::INCORRECT_SIZE;

	/// Serialize part
	size_t offset = 0;

	/// copy header part
	inputHeader(buffer, offset);

	memcpy(buffer + offset, data, header.length); // copy data
	offset += static_cast<size_t>(header.length);

	memcpy(buffer + offset, &end_mark, sizeof(end_mark)); // input end mark. Don't consider this length.
	offset += sizeof(end_mark);

	return ERROR_CODE::SUCCESS;
}


void Packet::copyHeader(const char* buffer, size_t& offset)
{
	int32_t netClientID, netType, netResult, netLength;

	memcpy(&netClientID, buffer + offset, sizeof(int32_t));
	offset += sizeof(int32_t);
	memcpy(&netType, buffer + offset, sizeof(int32_t));
	offset += sizeof(int32_t);
	memcpy(&netResult, buffer + offset, sizeof(int32_t));
	offset += sizeof(int32_t);
	memcpy(&netLength, buffer + offset, sizeof(int32_t));
	offset += sizeof(int32_t);

	header.clientID = ntohl(netClientID);
	header.type = static_cast<PacketType>(ntohl(netType));
	header.result = static_cast<PacketResult>(ntohl(netResult));
	header.length = ntohl(netLength);
}

ERROR_CODE Packet::deserialize(const char* buffer, int recvLength, size_t& offset)
{
	if (!buffer) return ERROR_CODE::GET_NULLPTR;
	size_t localOffset = offset; // local 복사

	if (localOffset + sizeof(PacketHeader) > recvLength) return ERROR_CODE::NEED_EXTRA_DATA;

	copyHeader(buffer, localOffset);

	if (header.length < 0) return ERROR_CODE::INCORRECT_SIZE;

	if (localOffset + header.length + sizeof(end_mark) > recvLength)
		return ERROR_CODE::NEED_EXTRA_DATA;

	memcpy(data, buffer + localOffset, header.length);
	localOffset += header.length;

	unsigned int received_end_mark = 0;
	memcpy(&received_end_mark, buffer + localOffset, sizeof(received_end_mark));

	if (received_end_mark != end_mark) {
		printf("received_end_mark: 0x%08X\n", received_end_mark); // hex 출력
		printf("header type: %d\n", header.type);
		printf("header length: %d\n", header.length);
		return ERROR_CODE::OPENED_PACKET;
	}
	localOffset += sizeof(end_mark);


	// 성공적으로 읽었으면 offset을 실제로 증가시킴
	offset = localOffset;
	return ERROR_CODE::SUCCESS;
}

void Packet::print_packet_contents(const char* where) {
	printf("\nPacket in[%s]\n", where);
	printf("Packet Type: %d [0x%08X]\n", header.type, header.type);
	printf("Packet result: %d [0x%08X]\n", header.result, header.result);
	printf("Packet Data (Length: %d): \n", header.length);

	for (size_t i = 0; i < header.length; ++i) printf("%02X ", data[i]);
	printf("\n");
}

