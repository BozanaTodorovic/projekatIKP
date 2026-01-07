#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "../Common/net.h"
#include <cstring>
#include <sstream>
#include <mutex>
#include <limits>

std::mutex qMutex;


#pragma comment(lib, "Ws2_32.lib")
void splitQuizMessage(const char* msg, char parts[7][256]) {
    int partIndex = 0;
    int charIndex = 0;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '|') {
            parts[partIndex][charIndex] = '\0';
            partIndex++;
            charIndex = 0;
            if (partIndex >= 7) break; //have 7 parts: quizId, qId, text, 4 questions
        }
        else {
            parts[partIndex][charIndex++] = msg[i];
            if (charIndex >= 255) charIndex = 255; //  overflow
        }
    }
    parts[partIndex][charIndex] = '\0';
}

int main(int argc, char* argv[]) {
    int subId = 0;
    if(argc>1){
        subId = std::atoi(argv[1]);
    }
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
    server.sin_port = htons(5000);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "connect() failed\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server!\n";
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;

    if (recvMsg(sock, type, payload, sizeof(payload) - 1, len)) {
        payload[len] = '\0';
        if (type == MsgType::QUIZ_LIST) {
            std::cout << "Available quizzes:\n";
            std::cout << payload << "\n";

            int selectedQuiz;
            std::cout << "Enter quiz ID to join: ";
            std::cin >> selectedQuiz;

            char buf[32];
            int subscriberId =subId;
            sprintf_s(buf, "%d %d", selectedQuiz, subscriberId);
            sendMsg(sock, MsgType::REGISTER, buf, (uint32_t)strlen(buf));
        }
    }
    /*const char* regPayload = "1 42"; // quizId=1, subId=42
    sendMsg(sock, MsgType::REGISTER, regPayload, (uint32_t)strlen(regPayload));*/

    MsgType type1;
    char buf[1024]{};
    uint32_t len1 = 0;

    // primi potvrdu registracije
    if (recvMsg(sock, type1, buf, sizeof(buf) - 1, len1)) {
        buf[len1] = '\0';
        std::cout << "Got msg type = " << (uint16_t)type1 << " payload=" << buf << "\n";
    }


    std::cout << "[SUBSCRIBER] Waiting for quiz to start...\n";

    bool quizRunning = true;
    while (quizRunning) {
        MsgType type;
        char buf[1024]{};
        uint32_t len;

        if (!recvMsg(sock, type, buf, sizeof(buf) - 1, len)) break;
        buf[len] = '\0';

        if (type == MsgType::QUIZ_QUESTION) {
            char parts[7][256]{};
            splitQuizMessage(buf, parts);

            std::cout << parts[2] << "\n";
            for (int i = 0; i < 4; i++)
                std::cout << i + 1 << ") " << parts[3 + i] << "\n";

            int answer;
            do {
                std::cout << "Your answer (1-4): ";
                std::cin >> answer;
            } while (answer < 1 || answer > 4);

            int zeroBased = answer;
            char buf[32];
            int subscriberId = subId;
            sprintf_s(buf, "%d", answer);
            sendMsg(sock,
                MsgType::QUIZ_ANSWER,
                buf,
                (uint32_t)strlen(buf));
            //break;
        }
        else if (type == MsgType::QUIZ_START) {
            std::cout << "[SUBSCRIBER] Quiz started!\n";
            std::string ans = "Quiz start";
            sendMsg(sock,
                MsgType::QUIZ_START,
                ans.c_str(),
                (uint32_t)ans.size());
        }
        else if (type == MsgType::QUIZ_RESULT) {
            std::cout << "[SUBSCRIBER] Quiz result \n";
            int subId = 0, quizId = 0, score = 0;
            sscanf_s(buf, "%d|%d|%d", &subId, &quizId, &score);
            std::cout << "Your result for quiz " << quizId << " is : " << score << std::endl;
        }
        else if (type == MsgType::QUIZ_WAIT_RESULT) {
            std::cout << "[SUBSCRIBER] Wait result...\n";
        }
        else if (type == MsgType::QUIZ_END) {
            std::cout << "[SUBSCRIBER] Quiz ended!\n";
            quizRunning = false; 
        }
    }
   
    std::cout << "Press ENTER to exit...\n";
    //ucita nam enter iz bafera pa onda prodje cin.get i ugasi window
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    closesocket(sock);
    WSACleanup();
    return 0;
}