#include "UDPserver.h"


UDPserver::UDPserver(USHORT port): udpSocket(INVALID_SOCKET), serverAddr{},
    port(port), exit_flag(false),
    logs(Logs::getInstance())
{
    // 1. Winsock √ ±‚»≠
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
        logs.err_exit(_T("WSAStartup() UDPserver"));
}

UDPserver::~UDPserver()
{
	WSACleanup();
	closesocket(udpSocket);
}

bool UDPserver::initialize()
{
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET)
        return logs.log_error("socket()", "UDPserver::initialize()");
    
    // 3. set the address information
    memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // get request from all interface


    // bind
    if (bind(udpSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
        return logs.log_error("bind()", "UDPserver::initialize()");

    return true;
}

void UDPserver::openServer()
{
    logs.log("Open the UDP server");

    while (!exit_flag)
    {
        try 
        {

        }

        catch (const char* msg)
        {
            logs.log_error(msg, "UDPserver::openServer()");
        }
    }
}