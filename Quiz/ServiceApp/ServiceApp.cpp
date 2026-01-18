#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "ServiceUtils.h"
#pragma comment(lib, "Ws2_32.lib")

int main() {
    std::cout << "====================  SERVICE  =====================\n\n" << std::endl;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return 1;

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(6000); // port servera
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) return 1;

    std::cout << "Service connected to server!\n";

    
    while (true) {
        MsgType type;
        char payload[1024]{};
        uint32_t len = 0;
        if (!recvMsg(sock, type, payload, sizeof(payload) - 1, len))
        {
            std::cout << "[SERVICE] Connection closed or recvMsg failed\n";
            break;
        }
        payload[len] = '\0';
        switch (type)
        {
        case MsgType::PING:{
            std::cout << "[SERVICE] Got PING\n";
            break;
        }
        case MsgType::QUIZ_START: {
            std::cout << "\n[SERVICE] Quiz started!\n";
            break;
        }
        case MsgType::CORRECT_ANSWER:{
            //std::cout << "[SERVICE] Correct answer: " << payload << "\n";
            //std::cout << "[SERVICE] Correct answer loaded \n"<<std::endl;
            addCorrectAnswer(payload);
            break;
        }case MsgType::QUIZ_ANSWER: {
            int subId = 0, qId = 0, answer = 0;
            int n = 0, quizId = 0;
            sscanf_s(payload, "%d|%d|%d|%d", &subId, &quizId, &qId, &answer, &n);
            std::cout << "\n[SERVICE] Received answer for quiz " << quizId<< "\n";
            processQuizAnswer(quizId, payload);
            break;
        }
        case MsgType::QUIZ_END: {
            int quiId = 0;
            sscanf_s(payload, "%d", &quiId);
            sendQuizResult(sock,quiId);
            std::cout << "[SERVICE] Quiz "<< quiId <<" ended! \n";
            break;
        }
        default: {
            std::cout << "[SERVICE] Unknown message type: " << (uint16_t)type << "\n";
            break;
        }
        }
    }


    std::cin.get();
    closesocket(sock);
    WSACleanup();
    return 0;
}
