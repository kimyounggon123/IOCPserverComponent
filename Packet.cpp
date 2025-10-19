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
	memcpy(data + offset, &str_len, sizeof(str_len));
	offset += sizeof(str_len);

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

	memcpy(data + offset, &str_len, sizeof(str_len));
	offset += sizeof(str_len);

	memcpy(data + offset, src.data(), str_len);
	offset += str_len;

	header.length = static_cast<int>(offset);
	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::copyData(void* dest, const size_t& data_size, size_t& offset) {
	if (!dest) return ERROR_CODE::GET_NULLPTR;
	if (offset + data_size > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	// copy data
	memcpy(dest, data + offset, data_size);
	offset += data_size;

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::copyString(char* dest, size_t& offset) {
	if (!dest) return ERROR_CODE::GET_NULLPTR;

	// check string flag
	bool is_utf8 = data[offset++] == 1;

	// read string length
	int str_len;
	memcpy(&str_len, data + offset, sizeof(int));
	offset += sizeof(int);

	// read real string
	if (offset + str_len > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	memcpy(dest, data + offset, str_len);
	offset += str_len;

	return ERROR_CODE::SUCCESS;
}
ERROR_CODE Packet::copyString(std::string& dest, size_t& offset) {

	// check string flag
	bool is_utf8 = data[offset++] == 1;

	// read string length
	int str_len;
	memcpy(&str_len, data + offset, sizeof(int));
	offset += sizeof(int);

	// read real string
	if (offset + str_len > BUFFERSIZE) return ERROR_CODE::INCORRECT_SIZE;

	dest.assign(data + offset, str_len);
	offset += str_len;

	return ERROR_CODE::SUCCESS;
}

/// <Serialize methods>
ERROR_CODE Packet::serialize(char* buffer, int& size) {
	// parameter buffer size를 체크해야 함

	if (!buffer) return ERROR_CODE::GET_NULLPTR;
	// if (is_header(PacketType::Default)) return ERROR_CODE::HAVETO_DELETE_PACKET_HEADER;

	// check size
	if (header.length < 0 ||
		sizeof(bool) + sizeof(PacketType) + sizeof(PacketResult) + sizeof(int) + header.length + sizeof(int) > BUFFERSIZE) // BUFFERSIZE = packet max size
		return ERROR_CODE::INCORRECT_SIZE;

	/// Serialize part
	size_t offset = 0;

	/// copy header part
	memcpy(buffer + offset, &header, sizeof(PacketHeader));
	offset += sizeof(PacketHeader);

	memcpy(buffer + offset, data, header.length); // copy data
	offset += static_cast<size_t>(header.length);

	memcpy(buffer + offset, &end_mark, sizeof(int)); // input end mark. Don't consider this length.
	offset += sizeof(int);
	size = static_cast<int>(offset);

	return ERROR_CODE::SUCCESS;
}

ERROR_CODE Packet::deserialize(const char* buffer, int recvLength, size_t& offset) {
	if (!buffer) return ERROR_CODE::GET_NULLPTR;
	if (offset + sizeof(PacketHeader) > recvLength) return ERROR_CODE::NEED_EXTRA_DATA;

	memset(this, 0, sizeof(Packet)); // clear packet

	/// copy header part
	memcpy(&header, buffer + offset, sizeof(PacketHeader));
	offset += sizeof(PacketHeader);

	// packet length check
	if (header.length < 0) return ERROR_CODE::INCORRECT_SIZE;

	// header + data + end mark > buffersize
	if (offset + header.length + sizeof(end_mark) > recvLength) return ERROR_CODE::NEED_EXTRA_DATA;

	// copy data
	memcpy(data, buffer + offset, header.length); // copy data (null char also be copied)
	offset += header.length;

	// 3. Verify End Mark
	unsigned int received_end_mark = 0;
	memcpy(&received_end_mark, buffer + offset, sizeof(received_end_mark));
	if (received_end_mark != end_mark)
	{
		printf("end_mark: 0x%08X\n", end_mark);
		printf("received_end_mark: 0x%08X\n", received_end_mark); // hex 출력
		return ERROR_CODE::OPENED_PACKET;
	}
	offset += sizeof(int);

	return ERROR_CODE::SUCCESS;
}


void Packet::print_packet_contents(const char* where) {
	printf("\nPacket in[%s]\n", where);
	printf("Marshal: %d\n", header.needToMarshal);
	printf("Packet Type: %d [0x%08X]\n", header.type, header.type);
	printf("Packet result: %d [0x%08X]\n", header.result, header.result);
	printf("Packet Data (Length: %d): \n", header.length);

	for (size_t i = 0; i < header.length; ++i) printf("%02X ", data[i]);
	printf("\n");
}

