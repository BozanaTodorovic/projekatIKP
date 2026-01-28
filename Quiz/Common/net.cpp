#include "pch.h"
#include "net.h"
#include <iostream>
#include <cstring>

bool sendAll(SOCKET s, const char* data, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(s, data + total, len - total, 0);
        if (sent == SOCKET_ERROR || sent == 0) return false;
        total += sent;
    }
    return true;
}

bool recvAll(SOCKET s, char* data, int len) {
    int total = 0;
    while (total < len) {
        int rec = recv(s, data + total, len - total, 0);
        if (rec == SOCKET_ERROR || rec == 0) return false;
        total += rec;
    }
    return true;
}

bool sendFrame(SOCKET s, const char* payload, uint32_t length) {
    uint32_t netLen = htonl(length);
    if (!sendAll(s, (const char*)&netLen, sizeof(netLen))) return false;
    if (length > 0) {
        if (!sendAll(s, payload, (int)length)) return false;
    }
    return true;
}

bool recvFrame(SOCKET s, char* outBuf, uint32_t maxBuf, uint32_t& outLen) {
    uint32_t netLen = 0;
    if (!recvAll(s, (char*)&netLen, sizeof(netLen))) return false;

    uint32_t length = ntohl(netLen);
    outLen = length;

    if (length == 0) return true;
    if (length > maxBuf) return false;

    return recvAll(s, outBuf, (int)length);
}

bool sendMsg(SOCKET s, MsgType type, const char* payload, uint32_t payloadLen) {
    uint32_t totalLen = (uint32_t)sizeof(uint16_t) + payloadLen;

    char temp[1024];
    if (totalLen > sizeof(temp)) return false;

    uint16_t t = (uint16_t)type;
    uint16_t netType = htons(t);

    memcpy(temp, &netType, sizeof(netType));

    if (payloadLen > 0 && payload != nullptr) {
        memcpy(temp + sizeof(netType), payload, payloadLen);
    }

    return sendFrame(s, temp, totalLen);
}

bool recvMsg(SOCKET s,MsgType& outType,char* outPayload,uint32_t maxBuf,uint32_t& outPayloadLen) {
    char temp[1024];
    uint32_t len = 0;

    if (!recvFrame(s, temp, (uint32_t)sizeof(temp), len)) return false;
    if (len < (uint32_t)sizeof(uint16_t)) return false;

    uint16_t netType = 0;
    memcpy(&netType, temp, sizeof(netType));

    uint16_t hostType = ntohs(netType);
    outType = (MsgType)hostType;

    uint32_t payloadLen = len - (uint32_t)sizeof(uint16_t);
    outPayloadLen = payloadLen;

    if (payloadLen == 0) return true;
    if (payloadLen > maxBuf) return false;

    memcpy(outPayload, temp + sizeof(uint16_t), payloadLen);
    return true;
}