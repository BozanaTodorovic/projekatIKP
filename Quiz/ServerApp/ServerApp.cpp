#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <sstream>

#include "../Common/net.h" // sendMsg / recvMsg
#pragma comment(lib, "Ws2_32.lib")

// ==================== DEFINICIJE ====================
enum QuizStatus { QUIZ_OPEN, QUIZ_RUNNING, QUIZ_FINISHED };
#define MAX_TOPIC_LEN 128
#define MAX_QUEST_LEN 256
#define MAX_OPTION_LEN 128
#define MAX_QUIZZES 64
#define MAX_QUESTIONS 32
#define MAX_SUBSCRIBERS 128
#define HASH_SIZE 256

// ==================== RUČNI STRUCTS ====================
struct Question {
    int questionId;
    char text[MAX_QUEST_LEN];
    char options[4][MAX_OPTION_LEN];
    int correctOption;
};

struct Subscriber {
    int subscriberId;
    SOCKET sock;
    int answers[MAX_QUESTIONS]; // -1 = not answered
};

struct Quiz {
    int quizId;
    char topic[MAX_TOPIC_LEN];
    QuizStatus status;
    time_t registrationDeadline;
    int quizDurationSeconds;

    Question questions[MAX_QUESTIONS];
    int questionCount;

    Subscriber subscribers[MAX_SUBSCRIBERS];
    int subscriberCount;
};

// ==================== KRUŽNI BAFERI ====================
struct QuizBuffer {
    Quiz buffer[MAX_QUIZZES];
    int head = 0;
    int tail = 0;
    int count = 0;

    void push(const Quiz& q) {
        buffer[tail] = q;
        tail = (tail + 1) % MAX_QUIZZES;
        if (count < MAX_QUIZZES) count++;
        else head = (head + 1) % MAX_QUIZZES; // prepisuje najstariji
    }

    Quiz* findById(int quizId) {
        for (int i = 0, idx = head; i < count; i++, idx = (idx + 1) % MAX_QUIZZES) {
            if (buffer[idx].quizId == quizId) return &buffer[idx];
        }
        return nullptr;
    }
};

QuizBuffer allQuizzes;
std::mutex quizMutex;

// ==================== HASH MAP ZA SUBSCRIBERE ====================
struct HashNode {
    int key; // subscriberId
    Subscriber value;
    HashNode* next;
};

struct HashMap {
    HashNode* buckets[HASH_SIZE];

    void init() { memset(buckets, 0, sizeof(buckets)); }

    int hash(int key) { return key % HASH_SIZE; }

    void insert(int key, const Subscriber& val) {
        int h = hash(key);
        HashNode* node = new HashNode{ key, val, buckets[h] };
        buckets[h] = node;
    }

    Subscriber* find(int key) {
        int h = hash(key);
        HashNode* node = buckets[h];
        while (node) {
            if (node->key == key) return &node->value;
            node = node->next;
        }
        return nullptr;
    }
};

HashMap subscriberMap;

// ==================== SOCKET GLOBAL ====================
SOCKET g_serviceSock = INVALID_SOCKET;
std::mutex serviceSockMutex;

// ==================== POMOĆNE FUNKCIJE ====================
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

void sendAnswerToService(int subscriberId, int quizId, int questionId, int answerIndex) {
    std::lock_guard<std::mutex> lock(serviceSockMutex);
    if (g_serviceSock == INVALID_SOCKET) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "%d|%d|%d|%d", subscriberId, quizId, questionId, answerIndex);
    sendMsg(g_serviceSock, MsgType::QUIZ_ANSWER, msg, (uint32_t)strlen(msg));
}
void sendQuizEndToService(int quizId) {
    std::lock_guard<std::mutex> lock(serviceSockMutex);
    if (g_serviceSock == INVALID_SOCKET) return;
    char msg[32];
    snprintf(msg, sizeof(msg), "%d", quizId);   // pretvori quizId u string
    sendMsg(g_serviceSock, MsgType::QUIZ_END, msg, (uint32_t)strlen(msg));
}

void sendCorrectAnswerToService(int quizId, int questionId, int correctAnswer) {
    std::lock_guard<std::mutex> lock(serviceSockMutex);
    if (g_serviceSock == INVALID_SOCKET) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "%d|%d|%d", quizId, questionId, correctAnswer);
    sendMsg(g_serviceSock, MsgType::CORRECT_ANSWER, msg, (uint32_t)strlen(msg));
}

// ==================== QUIZ LOGIKA ====================
void createQuiz(int quizId, int regDurationSec, int quizDurationSec, const char* topic) {
    std::lock_guard<std::mutex> lock(quizMutex);
    if (allQuizzes.findById(quizId)) return;

    Quiz q{};
    q.quizId = quizId;
    strncpy_s(q.topic, topic, MAX_TOPIC_LEN - 1);
    q.topic[MAX_TOPIC_LEN - 1] = '\0';
    q.status = QUIZ_OPEN;
    q.registrationDeadline = time(nullptr) + regDurationSec;
    q.quizDurationSeconds = quizDurationSec;
    q.questionCount = 0;
    q.subscriberCount = 0;

    allQuizzes.push(q);

    std::cout << "[SERVER] Quiz " << quizId << " created\n";
}

bool registerSubscriber(int quizId, int subId, SOCKET sock) {
    std::lock_guard<std::mutex> lock(quizMutex);
    Quiz* quiz = allQuizzes.findById(quizId);
    if (!quiz || quiz->status != QUIZ_OPEN || time(nullptr) > quiz->registrationDeadline)
        return false;

    if (quiz->subscriberCount >= MAX_SUBSCRIBERS) return false;

    Subscriber sub{};
    sub.subscriberId = subId;
    sub.sock = sock;
    for (int i = 0; i < MAX_QUESTIONS; i++) sub.answers[i] = -1;

    quiz->subscribers[quiz->subscriberCount++] = sub;
    subscriberMap.insert(subId, sub);

    std::cout << "[SERVER] Subscriber " << subId << " registered to quiz " << quizId << "\n";
    return true;
}

bool addQuestionToQuiz(int quizId, const Question& q) {
    std::lock_guard<std::mutex> lock(quizMutex);
    Quiz* quiz = allQuizzes.findById(quizId);
    if (!quiz || quiz->questionCount >= MAX_QUESTIONS) return false;

    quiz->questions[quiz->questionCount++] = q;

    std::cout << "[SERVER] Question " << q.questionId << " added to quiz " << quizId << "\n";
    return true;
}

void startQuiz(Quiz& q) {
    q.status = QUIZ_RUNNING;
    std::cout << "[SERVER] Quiz " << q.quizId << " STARTED\n";
   
    for (int i = 0; i < q.subscriberCount; i++) {
        sendMsg(q.subscribers[i].sock, MsgType::QUIZ_START, nullptr, 0);
    }
}

void quizTimerThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::lock_guard<std::mutex> lock(quizMutex);
        for (int i = 0; i < allQuizzes.count; i++) {
            int idx = (allQuizzes.head + i) % MAX_QUIZZES;
            Quiz& q = allQuizzes.buffer[idx];
            //std::cout << "Broj subscribera za kviz "<<q.quizId <<" je " << q.subscriberCount << std::endl;
            if (q.status == QUIZ_OPEN && time(nullptr) > q.registrationDeadline && q.subscriberCount>0) {
                startQuiz(q);
            }
        }
    }
}

// ==================== HANDLE SUBSCRIBER ====================
void handleSubscriber(SOCKET clientSock) {
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;
    int subscriberId = -1;
    int quizId = -1;
    Quiz* activeQuiz = nullptr;
    int currentQuestionIdx = 0;

    // Send list of quizzes
    {
        std::lock_guard<std::mutex> lock(quizMutex);
        char msg[4096];
        int pos = 0;
        for (int i = 0; i < allQuizzes.count; i++) {
            int idx = (allQuizzes.head + i) % MAX_QUIZZES;
            Quiz& q = allQuizzes.buffer[idx];
            pos += snprintf(msg + pos, sizeof(msg) - pos, "%d)%s\n", q.quizId, q.topic);
        }
        sendMsg(clientSock, MsgType::QUIZ_LIST, msg, (uint32_t)strlen(msg));
    }

    while (true) {
        if (!recvMsg(clientSock, type, payload, sizeof(payload) - 1, len)) {
            std::cout << "[SERVER] Subscriber disconnected\n";
            break;
        }
        payload[len] = '\0';

        switch (type) {
        case MsgType::PING: {
            sendMsg(clientSock, MsgType::PONG, nullptr, 0);
            break;
        }

        case MsgType::REGISTER: {
            if (sscanf_s(payload, "%d %d", &quizId, &subscriberId) == 2) {
                bool ok = registerSubscriber(quizId, subscriberId, clientSock);
                sendMsg(clientSock, ok ? MsgType::REGISTER_OK : MsgType::REGISTER_DENIED,
                    ok ? "OK" : "DENIED", ok ? 2 : 6);
                if (ok) {
                    activeQuiz = allQuizzes.findById(quizId);
                }
            }
            else {
                sendMsg(clientSock, MsgType::REGISTER_DENIED, "BAD_FORMAT", 10);
            }
            break;
        }

        case MsgType::QUIZ_START: {
            if (!activeQuiz) break;
            currentQuestionIdx = 0;
            if (activeQuiz->questionCount > 0) {
                Question& q = activeQuiz->questions[currentQuestionIdx];
                char msg[1024];
                snprintf(msg, sizeof(msg), "%d|%d|%s|%s|%s|%s|%s",
                    activeQuiz->quizId, q.questionId, q.text,
                    q.options[0], q.options[1], q.options[2], q.options[3]);
                sendMsg(clientSock, MsgType::QUIZ_QUESTION, msg, (uint32_t)strlen(msg));
            }
            break;
        }

        case MsgType::QUIZ_ANSWER: {
            if (!activeQuiz || currentQuestionIdx >= activeQuiz->questionCount) break;

            int ans = atoi(payload);
            Subscriber* sub = subscriberMap.find(subscriberId);
            if (!sub) {
                std::cout << "[SERVER] Subscriber not found in map! ID: " << subscriberId << "\n";
            }
            else {
                if (currentQuestionIdx < MAX_QUESTIONS)  // zaštita od out-of-bounds
                    sub->answers[currentQuestionIdx] = ans;
            }
            sendAnswerToService(subscriberId, activeQuiz->quizId,
                activeQuiz->questions[currentQuestionIdx].questionId, ans);

            currentQuestionIdx++;

            if (currentQuestionIdx < activeQuiz->questionCount) {
                Question& q = activeQuiz->questions[currentQuestionIdx];
                char msg[1024];
                snprintf(msg, sizeof(msg), "%d|%d|%s|%s|%s|%s|%s",
                    activeQuiz->quizId, q.questionId, q.text,
                    q.options[0], q.options[1], q.options[2], q.options[3]);
                sendMsg(clientSock, MsgType::QUIZ_QUESTION, msg, (uint32_t)strlen(msg));
            }
            else {
                sendMsg(clientSock, MsgType::QUIZ_WAIT_RESULT, nullptr, 0);
                //std::cout << "Quiz id je " << quizId << std::endl;
                sendQuizEndToService(quizId);
            }
            break;
        }
        case MsgType::DISCONNECT: {
            goto cleanup;
        }

        default: {
            std::cout << "[SERVER] Unknown MsgType: " << (uint16_t)type << "\n";
            break;
        }
        }
    }

cleanup:
    closesocket(clientSock);
    std::cout << "[SERVER] Subscriber closed socket\n";
}


// ==================== HANDLE PUBLISHER ====================
void handlePublisher(SOCKET pubSock) {
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;

    while (true) {
        if (!recvMsg(pubSock, type, payload, sizeof(payload) - 1, len)) break;
        payload[len] = '\0';

        switch (type) {
        case MsgType::CREATE_QUIZ: {
            int quizId, regSec, durSec;
            char topic[128]{};
            if (sscanf_s(payload, "%d %d %d %127s", &quizId, &regSec, &durSec, topic, (unsigned)_countof(topic)) >= 3) {
                createQuiz(quizId, regSec, durSec, topic);
                sendMsg(pubSock, MsgType::CREATE_QUIZ_ACK, "OK", 2);
            }
            else sendMsg(pubSock, MsgType::CREATE_QUIZ_ACK, "BAD_FORMAT", 10);
            break;
        }
        case MsgType::ADD_QUESTION: {
            int quizId, qId, correct;
            char text[256]{}, o1[128]{}, o2[128]{}, o3[128]{}, o4[128]{};
            if (sscanf_s(payload, "%d|%d|%255[^|]|%127[^|]|%127[^|]|%127[^|]|%127[^|]|%d",
                &quizId, &qId, text, (unsigned)_countof(text),
                o1, (unsigned)_countof(o1), o2, (unsigned)_countof(o2),
                o3, (unsigned)_countof(o3), o4, (unsigned)_countof(o4), &correct) == 8) {
                Question q{};
                q.questionId = qId;
                strncpy_s(q.text, text, MAX_QUEST_LEN - 1);
                strncpy_s(q.options[0], o1, MAX_OPTION_LEN - 1);
                strncpy_s(q.options[1], o2, MAX_OPTION_LEN - 1);
                strncpy_s(q.options[2], o3, MAX_OPTION_LEN - 1);
                strncpy_s(q.options[3], o4, MAX_OPTION_LEN - 1);
                q.correctOption = correct;
                addQuestionToQuiz(quizId, q);
                sendCorrectAnswerToService(quizId, qId, correct);
                sendMsg(pubSock, MsgType::ADD_QUESTION_ACK, "OK", 2);
            }
            else sendMsg(pubSock, MsgType::ADD_QUESTION_ACK, "BAD_FORMAT", 10);
            break;
        }
        case MsgType::DISCONNECT:
            goto cleanup;
        default:
            std::cout << "[SERVER] Unknown MsgType from publisher: " << (uint16_t)type << "\n";
        }
    }

cleanup:
    closesocket(pubSock);
    std::cout << "[SERVER] Publisher disconnected\n";
}

// ==================== HANDLE PUBLISHER ====================
void handleService(SOCKET serviceSock) {
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;

    while (true) {
        if (!recvMsg(serviceSock, type, payload, sizeof(payload) - 1, len)) break;
        payload[len] = '\0';

        switch (type) {
        case MsgType::QUIZ_RESULT: {
            int subId = 0, quizId = 0, score = 0;
            sscanf_s(payload, "%d|%d|%d", &subId, &quizId, &score);
            // 2️⃣ Pošalji subscriberu ako je trenutno spojen
            Subscriber* sub = subscriberMap.find(subId); // trebaš funkciju koja traži subscriber
            if (sub) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%d|%d|%d", subId, quizId, score);
                std::cout << msg << std::endl;
                sendMsg(sub->sock, MsgType::QUIZ_RESULT, msg, (uint32_t)strlen(msg));
                sendMsg(sub->sock, MsgType::QUIZ_END, nullptr, 0);
            }
            break;
        }
        case MsgType::DISCONNECT:
            goto cleanup;
        default:
            std::cout << "[SERVICE] Unknown MsgType from service: " << (uint16_t)type << "\n";
        }
    }

cleanup:
    closesocket(serviceSock);
    std::cout << "[SERVICE] Service disconnected\n";
}


// ==================== MAIN ====================
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
