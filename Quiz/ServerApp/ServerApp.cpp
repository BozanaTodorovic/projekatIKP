#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include "Handlers.h"
#pragma comment(lib, "Ws2_32.lib")

SOCKET createListenSocket(uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    if (listen(s, 5) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
    subscriberMap.init();

    std::thread(quizTimerThread).detach();

    SOCKET listenService = createListenSocket(6000);
    if (listenService == INVALID_SOCKET) return 1;
    std::thread([&]() {
        while (true) {
            SOCKET serviceSock = accept(listenService, nullptr, nullptr);
            if (serviceSock != INVALID_SOCKET) {
                { 
                    std::lock_guard<std::mutex> lk(serviceSockMutex); g_serviceSock = serviceSock; 
                }
                sendMsg(serviceSock, MsgType::PING, nullptr, 0);
                std::thread(handleService, serviceSock).detach();
            }
        }
        }).detach();

    std::cout << "Service connected\n";

    // Publisher socket
    SOCKET listenPublisher = createListenSocket(5001);
    if (listenPublisher == INVALID_SOCKET) return 1;
    std::thread([&]() {
        while (true) {
            SOCKET pubSock = accept(listenPublisher, nullptr, nullptr);
            if (pubSock != INVALID_SOCKET) std::thread(handlePublisher, pubSock).detach();
        }
        }).detach();

    // Subscriber socket
    SOCKET listenSub = createListenSocket(5000);
    if (listenSub == INVALID_SOCKET) return 1;
    while (true) {
        SOCKET clientSock = accept(listenSub, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET) std::thread(handleSubscriber, clientSock).detach();
    }

    std::cout << "Press ENTER to exit...\n";
    std::cin.get();
    closesocket(listenService);
    //closesocket(serviceSock);
    closesocket(listenPublisher);
    closesocket(listenSub);
    WSACleanup();
    return 0;
}
