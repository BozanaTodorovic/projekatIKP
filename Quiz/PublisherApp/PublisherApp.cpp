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
    char text[256];
    char options[4][128];
    int correctOption;
};
bool sendQuestion(SOCKET sock, int quizId, const QuestionToSend& q) {
    char payload[1024]{};

    sprintf_s(payload, sizeof(payload),
        "%d|%d|%s|%s|%s|%s|%s|%d",
        quizId,
        q.questionId,
        q.text,
        q.options[0],
        q.options[1],
        q.options[2],
        q.options[3],
        q.correctOption
    );

    if (!sendMsg(sock, MsgType::ADD_QUESTION,
        payload, (uint32_t)strlen(payload))) {
        std::cout << "Failed to send ADD_QUESTION\n";
        return false;
    }

    MsgType type;
    char buf[256]{};
    uint32_t len = 0;

    if (recvMsg(sock, type, buf, sizeof(buf) - 1, len)) {
        buf[len] = '\0';
        return (type == MsgType::ADD_QUESTION_ACK);
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
    QuestionToSend quiz1Questions[] = {
    {1, "Koji je glavni grad Srbije?",
        {"Beograd","Nis","Novi Sad","Kragujevac"}, 1},
    {2, "Koja boja nastaje mesanjem plave i žute?",
        {"Crvena","Zelena","Plava","Žuta"}, 2},
    {3, "Koliko planeta ima Suncev sistem?",
        {"7","8","9","10"}, 1}
    };

    int quiz1Count = sizeof(quiz1Questions) / sizeof(quiz1Questions[0]);

    for (int i = 0; i < quiz1Count; i++) {
        sendQuestion(sock, 1, quiz1Questions[i]);
    }
    QuestionToSend quiz2Questions[] = {
      {1, "Koji je glavni grad Madjarske?",
          {"Budimpesta","Nis","Novi Sad","Kragujevac"}, 0},
      {2, "Koja boja nastaje mesanjem crvene i zute?",
          {"Narandzasta","Zelena","Plava","Zuta"}, 0},
      {3, "Koliko planeta ima Suncev sistem?",
          {"7","8","9","10"}, 1}
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
    int quiz2Count = sizeof(quiz2Questions) / sizeof(quiz2Questions[0]);

    for (int i = 0; i < quiz2Count; i++) {
        sendQuestion(sock, 2, quiz2Questions[i]);
    }

    std::cout << "Press ENTER to exit...\n";
    std::cin.get();

    closesocket(sock);
    WSACleanup();
    return 0;
}
