#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <sstream>  // za stringstream
#include "Utils.h"
#pragma comment(lib, "Ws2_32.lib")


int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cout << "socket() failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(5001);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "connect() failed\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Publisher connected!\n";
    Quizes q[3];
    setQuizes(q);
    for (int i = 0;i < 3;i++) {

        char payload[256]{};

        sprintf_s(payload, sizeof(payload), "%d %d %s", 20, 30, q[i].topic);
        sendMsg(sock, MsgType::CREATE_QUIZ, payload, (uint32_t)strlen(payload));
        MsgType type;
        char buf[256]{};
        int quizId = 0;
        uint32_t len = 0;
        if (recvMsg(sock, type, buf, sizeof(buf) - 1, len)) {
            buf[len] = '\0';
            std::cout << "Publisher got CREATE_QUIZ response: type="
                << (uint16_t)type << " payload=" << buf << "\n";
            sscanf_s(buf, "%d", &quizId);
        }
        int quiz1Count = 3;


        for (int j = 0; j < quiz1Count; j++) {
            sendQuestion(sock, quizId, q[i].questions[j]);
        }
    }

    std::cout << "Press ENTER to exit...\n";
    std::cin.get();

    closesocket(sock);
    WSACleanup();
    return 0;
}
