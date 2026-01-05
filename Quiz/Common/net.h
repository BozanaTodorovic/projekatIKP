#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include "protocol.h"

// šalje tacno len bajtova (vrati false ako pukne)
bool sendAll(SOCKET s, const char* data, int len);

// primi tacno len bajtova (vrati false ako pukne)
bool recvAll(SOCKET s, char* data, int len);

// šalje “uokvirenu” poruku: [uint32 length][payload]
bool sendFrame(SOCKET s, const char* payload, uint32_t length);

// primi “uokvirenu” poruku, alocira buffer (ti proslediš vec alociran)
// ovdje cemo u praksi primati u std::string kasnije, ali za sada prosto:
// vrati length u outLen, a payload napuni u outBuf
bool recvFrame(SOCKET s, char* outBuf, uint32_t maxBuf, uint32_t& outLen);

// šalje poruku: [MsgType][payload]
bool sendMsg(SOCKET s, MsgType type, const char* payload, uint32_t payloadLen);

// prima poruku: izvuce MsgType i payload
bool recvMsg(
    SOCKET s,
    MsgType& outType,
    char* outPayload,
    uint32_t maxBuf,
    uint32_t& outPayloadLen
);

