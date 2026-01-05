#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include "../Common/protocol.h"
#include "../Common/net.h"

#pragma comment(lib, "Ws2_32.lib")

// =================== Include prethodne definicije ===================

struct CorrectAnswer {
    int questionId;
    int correctOption;
};

struct RingBuffer {
    CorrectAnswer* buffer;
    size_t capacity;
    size_t start = 0;
    size_t size = 0;

    RingBuffer(size_t cap) : capacity(cap) { buffer = new CorrectAnswer[cap]; }
    ~RingBuffer() { delete[] buffer; }

    void push(const CorrectAnswer& ca) {
        buffer[(start + size) % capacity] = ca;
        if (size < capacity) size++;
        else start = (start + 1) % capacity;
    }

    bool get(int questionId, int& correctOption) {
        for (size_t i = 0; i < size; i++) {
            size_t idx = (start + i) % capacity;
            if (buffer[idx].questionId == questionId) {
                correctOption = buffer[idx].correctOption;
                return true;
            }
        }
        return false;
    }
};

struct SubResult {
    int subscriberId;
    int score;
    SubResult* next;
};

struct HashMap {
    SubResult** buckets;
    size_t capacity;

    HashMap(size_t cap) : capacity(cap) {
        buckets = new SubResult * [cap];
        for (size_t i = 0; i < cap; i++) buckets[i] = nullptr;
    }

    ~HashMap() {
        for (size_t i = 0; i < capacity; i++) {
            SubResult* curr = buckets[i];
            while (curr) { SubResult* tmp = curr; curr = curr->next; delete tmp; }
        }
        delete[] buckets;
    }

    size_t hash(int subscriberId) { return subscriberId % capacity; }

    void addOrUpdate(int subscriberId, int points) {
        size_t idx = hash(subscriberId);
        SubResult* curr = buckets[idx];
        while (curr) {
            if (curr->subscriberId == subscriberId) { curr->score += points; return; }
            curr = curr->next;
        }
        SubResult* newSub = new SubResult{ subscriberId, points, buckets[idx] };
        buckets[idx] = newSub;
    }

    void printAll() {
        for (size_t i = 0; i < capacity; i++) {
            SubResult* curr = buckets[i];
            while (curr) { std::cout << "Sub " << curr->subscriberId << " -> " << curr->score << " pts\n"; curr = curr->next; }
        }
    }
};

struct QuizResultNode {
    int quizId;
    HashMap* subResults;
    RingBuffer* correctAnswers;
    QuizResultNode* next;
};

QuizResultNode* allQuizzes = nullptr;

QuizResultNode* getOrCreateQuiz(int quizId) {
    QuizResultNode* curr = allQuizzes;
    while (curr) { if (curr->quizId == quizId) return curr; curr = curr->next; }
    QuizResultNode* newQuiz = new QuizResultNode;
    newQuiz->quizId = quizId;
    newQuiz->subResults = new HashMap(100);
    newQuiz->correctAnswers = new RingBuffer(100);
    newQuiz->next = allQuizzes;
    allQuizzes = newQuiz;
    return newQuiz;
}

void addCorrectAnswer(const char* payload) {
    const char* line = payload;
    while (*line) {
        int quizId = 0, qId = 0, correctAnswer = 0;
        int n = 0;
        sscanf_s(line, "%d|%d|%d%n", &quizId, &qId, &correctAnswer, &n);
        line += n; if (*line == '\n') line++;
        QuizResultNode* quiz = getOrCreateQuiz(quizId);
        CorrectAnswer ca{ qId, correctAnswer };
        quiz->correctAnswers->push(ca);
        std::cout << "Dodat tacan odgovor" << correctAnswer << " for qID" << qId << ":\n";

    }
}

void processQuizAnswer(int quizId, const char* payload) {
    QuizResultNode* quiz = getOrCreateQuiz(quizId);
    const char* line = payload;
    while (*line) {
        int subId = 0, qId = 0, answer = 0;
        int n = 0;
        sscanf_s(line, "%d|%d|%d|%d%n", &subId, &quizId, &qId, &answer, &n);
        int correctOption; int points = 0;
        if (quiz->correctAnswers->get(qId, correctOption)) {
            if (answer == correctOption) points = 2;
        }
        std::cout << "Question points for qID is" << qId << ":\n";
        quiz->subResults->addOrUpdate(subId, points);
        line += n; if (*line == '\n') line++;
    }
    std::cout << "Updated scores for quiz " << quizId << ":\n";
    quiz->subResults->printAll();
}



// ===================== Main =====================
int main() {
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

    // Za test: dodajmo neke tacne odgovore
    /*addCorrectAnswer(1, 1, 2);
    addCorrectAnswer(1, 2, 0);
    addCorrectAnswer(1, 3, 1);
    */
    // Glavna petlja primanja poruka
    /*while (true) {
        MsgType type;
        char payload[1024]{};
        uint32_t len = 0;

        if (!recvMsg(sock, type, payload, sizeof(payload) - 1, len)) break;
        payload[len] = '\0';

        switch (type) {
        case MsgType::PING: std::cout << "PING received\n"; break;
        case MsgType::QUIZ_ANSWER:
            std::cout << "QUIZ_ANSWER received\n";
            processQuizAnswer(1, payload);
            break;
        default:
            std::cout << "Unknown MsgType: " << (uint16_t)type << "\n";
        }
    }*/
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
        case MsgType::PING:
            std::cout << "[SERVICE] Got PING\n";
            break;
        case MsgType::QUIZ_START:
            std::cout << "[SERVICE] Quiz started!\n";
            break;
        case MsgType::CORRECT_ANSWER:
            std::cout << "[SERVICE] Correct answer: " << payload << "\n";
            addCorrectAnswer(payload);
            break;
        case MsgType::QUIZ_ANSWER:
            std::cout << "[SERVICE] Received answer: " << payload << "\n";
            processQuizAnswer(1, payload);
            break;
        default:
            std::cout << "[SERVICE] Unknown message type: " << (uint16_t)type << "\n";
            break;
        }
    }



    std::cin.get();
    closesocket(sock);
    WSACleanup();
    return 0;
}
