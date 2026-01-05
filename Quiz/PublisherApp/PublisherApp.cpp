#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include "../Common/net.h"
#include <sstream>  // za stringstream
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
struct QuestionToSend {
    int questionId;
    std::string text;
    std::string options[4];
    int correctOption;
};

bool sendQuestion(SOCKET sock, int quizId, const QuestionToSend& q) {
    std::stringstream ss;
    ss << quizId << "|" << q.questionId << "|"
        << q.text << "|"
        << q.options[0] << "|"
        << q.options[1] << "|"
        << q.options[2] << "|"
        << q.options[3] << "|"
        << q.correctOption;

    std::string payload = ss.str();

    // koristi sendMsg koji šalje tip poruke + dužinu + payload
    if (!sendMsg(sock, MsgType::ADD_QUESTION, payload.c_str(), (uint32_t)payload.size())) {
        std::cout << "Failed to send ADD_QUESTION\n";
        return false;
    }

    // primi ACK od servera
    MsgType type;
    char buf[256]{};
    uint32_t len = 0;
    if (recvMsg(sock, type, buf, sizeof(buf) - 1, len)) {
        buf[len] = '\0';
        if (type == MsgType::ADD_QUESTION_ACK) {
            std::cout << "Server ACK: " << buf << "\n";
            return true;
        }
        else {
            std::cout << "Unexpected server response: type=" << (uint16_t)type
                << " payload=" << buf << "\n";
        }
    }
    else {
        std::cout << "Failed to receive ACK\n";
    }

    return false;
}


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

    // payload = "quizId topic"
    const char* payload = "1 20 30 OpsteZnanje";
    sendMsg(sock, MsgType::CREATE_QUIZ, payload, (uint32_t)strlen(payload));

    MsgType type;
    char buf[256]{};
    uint32_t len = 0;
    if (recvMsg(sock, type, buf, sizeof(buf) - 1, len)) {
        buf[len] = '\0';
        std::cout << "Publisher got CREATE_QUIZ response: type="
            << (uint16_t)type << " payload=" << buf << "\n";
    }

    // 2) pitanja
    std::vector<QuestionToSend> questions = {
        {1, "Koji je glavni grad Srbije?", {"Beograd","Nis","Novi Sad","Kragujevac"}, 1},
        {2, "Koja boja nastaje mesanjem plave i žute?", {"Crvena","Zelena","Plava","Žuta"}, 2},
        {3, "Koliko planeta ima Suncev sistem?", {"7","8","9","10"}, 1}
    };
    for (const auto& q : questions) {
        if (!sendQuestion(sock, 1, q)) {  // 1 = quizId
            std::cout << "Question " << q.questionId << " failed to send\n";
        }
    }
    std::vector<QuestionToSend> questions1 = {
      {1, "Koji je glavni grad Madjarske?", {"Beograd","Nis","Novi Sad","Kragujevac"}, 1},
      {2, "Koja boja nastaje mešanjem crevene i žute?", {"Crvena","Zelena","Plava","Žuta"}, 2},
      {3, "Koliko planeta ima sistema ?", {"7","8","9","10"}, 1}
    };

    const char* payload1 = "2 20 30 OpsteZnanje2";
    sendMsg(sock, MsgType::CREATE_QUIZ, payload1, (uint32_t)strlen(payload1));
    MsgType type1;
    char buf1[256]{};
    uint32_t len1 = 0;
    if (recvMsg(sock, type1, buf1, sizeof(buf1) - 1, len1)) {
        buf[len1] = '\0';
        std::cout << "Publisher got CREATE_QUIZ response: type="
            << (uint16_t)type1 << " payload=" << buf1 << "\n";
    }
    for (const auto& q : questions1) {
        if (!sendQuestion(sock, 2, q)) {  // 1 = quizId
            std::cout << "Question " << q.questionId << " failed to send\n";
        }
    }

    std::cout << "Press ENTER to exit...\n";
    std::cin.get();

    closesocket(sock);
    WSACleanup();
    return 0;
}
