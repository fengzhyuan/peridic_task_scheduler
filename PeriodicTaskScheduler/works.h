#ifndef _WORKS_H_
#define _WORKS_H_


#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#pragma warning(disable: 4996 )
#pragma warning(disable: 4244 )

const char *get_ip(const char *host_name) {
	WSADATA wsaData; int iResult; DWORD dwError;
	struct hostent *remoteHost; struct in_addr addr;
	
	try {
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			printf("WSAStartup failed: %d\n", iResult); throw exception();
		}
		remoteHost = gethostbyname(host_name);
		if (remoteHost == NULL) {
			dwError = WSAGetLastError();
			if (dwError != 0) {
				if (dwError == WSAHOST_NOT_FOUND) {
					printf("Host not found\n"); throw exception();
				}
				else if (dwError == WSANO_DATA) {
					printf("No data record found\n"); throw exception();
				}
				else {
					printf("Function failed with error: %ld\n", dwError); throw exception();
				}
			}
		}
		else {
			addr.s_addr = *(u_long *)remoteHost->h_addr_list[0];
			return inet_ntoa(addr);
		}
	}
	catch (...) {
		return nullptr;
	}
	return nullptr;
}

/*
https://msdn.microsoft.com/en-us/library/windows/desktop/aa366050(v=vs.85).aspx
*/
float icmp_ping(const char *host_name) {
	const char *ip = get_ip(host_name);
	HANDLE hIcmpFile;
	unsigned long ipaddr = INADDR_NONE;
	DWORD dwRetVal = 0;
	char SendData[32] = "Data Buffer";
	LPVOID ReplyBuffer = NULL;
	DWORD ReplySize = 0;

	float elapsed = 0;

	try {
		ipaddr = inet_addr(ip);
		if (ipaddr == INADDR_NONE) {
			throw exception("INADDR_NONE");
		}

		hIcmpFile = IcmpCreateFile();
		if (hIcmpFile == INVALID_HANDLE_VALUE) {
			throw exception("INVALID_HANDLE_VALUE");
		}

		ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
		ReplyBuffer = (VOID*)malloc(ReplySize);
		if (ReplyBuffer == NULL) {
			throw exception("\tUnable to allocate memory\n");
		}

		dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof(SendData),
			NULL, ReplyBuffer, ReplySize, 1000);
		if (dwRetVal != 0) {
			PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
			struct in_addr ReplyAddr;
			ReplyAddr.S_un.S_addr = pEchoReply->Address;
			elapsed = pEchoReply->RoundTripTime;
		}
		else {
			throw exception("dwRetVal=0");
		}
	}
	catch (...) {
		elapsed = -1;
	}
	if (ReplyBuffer) free(ReplyBuffer);
	return elapsed;
}

/*
http://www.cs.kent.edu/~ruttan/sysprog/lectures/sockets/winSocketClient.cpp
*/
#define DEFAULT_BUFLEN 512
float tcp_connect(const char *host_name, const char *port) {
	WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    char *sendbuf = "test message", recvbuf[DEFAULT_BUFLEN];
    int iResult, recvbuflen = DEFAULT_BUFLEN;
	float elapsed = -1;

	auto start_t = chrono::high_resolution_clock::now(), end_t = start_t;
	try {
		// Initialize Winsock
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			throw exception("WSAStartup failed with error: %d\n");
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and port
		iResult = getaddrinfo(host_name, port, &hints, &result);
		if (iResult != 0) {
			throw exception("getaddrinfo failed with error\n");
			WSACleanup();
		}

		// Attempt to connect to an address until one succeeds
		for (ptr = result; ptr != NULL;ptr = ptr->ai_next) {
			// Create a SOCKET for connecting to server
			ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol);
			if (ConnectSocket == INVALID_SOCKET) {
				throw exception("socket failed with error: %ld\n");
				WSACleanup();
			}

			// Connect to server.
			iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				ConnectSocket = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(result);

		if (ConnectSocket == INVALID_SOCKET) {
			throw exception("Unable to connect to server!\n");
			WSACleanup();
		}

		// Send an initial buffer
		iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iResult == SOCKET_ERROR) {
			throw exception("send failed with error\n");
			closesocket(ConnectSocket);
			WSACleanup();
		}

		// shutdown the connection since no more data will be sent
		iResult = shutdown(ConnectSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			throw exception("shutdown failed with error\n");
			closesocket(ConnectSocket);
			WSACleanup();
		}

		// Receive until the peer closes the connection
		do {
			iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			if (iResult < 0) throw exception("recv failed with error\n");
		} while (iResult > 0);

		// cleanup
		closesocket(ConnectSocket);
		WSACleanup();
		end_t = chrono::high_resolution_clock::now();
		elapsed = elapsed = chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t).count();
	}
	catch (...) {
		elapsed = -1;
	}
	return elapsed;
}

#endif
