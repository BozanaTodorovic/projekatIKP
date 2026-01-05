#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <map>
#include <vector>

#include "../Common/net.h" // tvoj sendMsg / recvMsg
#pragma comment(lib, "Ws2_32.lib")

enum QuizStatus { QUIZ_OPEN, QUIZ_RUNNING, QUIZ_FINISHED };

struct SubscriberInfo {
    int subscriberId;
    SOCKET socket;
};

struct SubNode {
    SubscriberInfo sub;
    SubNode* next;
};

struct Question {
    int questionId;
    std::string text;
    std::string options[4];
    int correctOption;
    Question* next = nullptr;
};

struct Quiz {
    int quizId;
    char topic[128];
    QuizStatus status;
    time_t registrationDeadline;
    int quizDurationSeconds;
    SubNode* subscribers = nullptr;
    Question* questions = nullptr;
    std::map<int, std::map<int, int>> answers; // subscriberId -> questionId -> answerIndex
};

struct QuizNode {
    Quiz quiz;
    QuizNode* next;
};

QuizNode* allQuizzes = nullptr;
std::mutex quizMutex;

SOCKET g_serviceSock = INVALID_SOCKET;
std::mutex serviceSockMutex;

SOCKET g_subSock = INVALID_SOCKET;
std::mutex subSockMutex;


// ==================== Socket helpers ====================

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

// ==================== Quiz helpers ====================

Quiz* findQuizById_NoLock(int quizId) {
    QuizNode* curr = allQuizzes;
    while (curr) {
        if (curr->quiz.quizId == quizId) return &curr->quiz;
        curr = curr->next;
    }
    return nullptr;
}

void createQuiz(int quizId, int regDurationSec, int quizDurationSec, const char* topic) {
    std::lock_guard<std::mutex> lock(quizMutex);
    if (findQuizById_NoLock(quizId)) return;

    QuizNode* node = new QuizNode;
    node->quiz.quizId = quizId;
    strncpy_s(node->quiz.topic, topic, sizeof(node->quiz.topic) - 1);
    node->quiz.topic[sizeof(node->quiz.topic) - 1] = '\0';
    node->quiz.status = QUIZ_OPEN;
    node->quiz.registrationDeadline = time(nullptr) + regDurationSec;
    node->quiz.quizDurationSeconds = quizDurationSec;
    node->quiz.subscribers = nullptr;
    node->next = allQuizzes;
    allQuizzes = node;

    std::cout << "[SERVER] Quiz " << quizId << " created\n";
}

bool registerSubscriber(int quizId, int subId, SOCKET sock) {
    std::lock_guard<std::mutex> lock(quizMutex);
    Quiz* quiz = findQuizById_NoLock(quizId);
    if (!quiz || quiz->status != QUIZ_OPEN || time(nullptr) > quiz->registrationDeadline)
        return false;

    SubNode* node = new SubNode;
    node->sub.subscriberId = subId;
    node->sub.socket = sock;
    node->next = quiz->subscribers;
    quiz->subscribers = node;

    std::cout << "[SERVER] Subscriber " << subId << " registered to quiz " << quizId << "\n";
    return true;
}

void addQuestionToQuiz(int quizId, const Question& q) {
    std::lock_guard<std::mutex> lock(quizMutex);
    Quiz* quiz = findQuizById_NoLock(quizId);
    if (!quiz) return;

    Question* node = new Question(q);
    node->next = quiz->questions;
    quiz->questions = node;

    std::cout << "[SERVER] Question " << q.questionId << " added to quiz " << quizId << "\n";
}

// ==================== Start quiz ====================
/*void runQuizForSubscriber(Quiz& quiz, SubNode* sub) {
    SOCKET sock = sub->sub.socket;
    int subId = sub->sub.subscriberId;

    sendMsg(sock, MsgType::QUIZ_START, nullptr, 0);

    Question* q = quiz.questions;
    while (q) {
        // 1️⃣ pošalji pitanje
        std::stringstream ss;
        ss << quiz.quizId << "|" << q->questionId << "|"
            << q->text << "|"
            << q->options[0] << "|"
            << q->options[1] << "|"
            << q->options[2] << "|"
            << q->options[3];

        std::string payload = ss.str();
        sendMsg(sock, MsgType::QUIZ_QUESTION, payload.c_str(), (uint32_t)payload.size());

        // 2️⃣ čekaj odgovor
        MsgType type;
        char buf[64]{};
        uint32_t len = 0;

        if (!recvMsg(sock, type, buf, sizeof(buf) - 1, len)) {
            std::cout << "[DEBUG] Received:"
                << " type=" << (uint16_t)type
                << " len=" << len
                << " payload='" << buf << "'\n";
            std::cout << "[SERVER] Subscriber lost connection\n";
            return;
        }

        buf[len] = '\0';
        std::cout << "[DEBUG] Nije uslo u if ovo je pred sledeci if:"
            << " type=" << (uint16_t)type
            << " len=" << len
            << " payload='" << buf << "'\n";
        if (type == MsgType::QUIZ_ANSWER) {
            int answerIndex = atoi(buf);
            std::lock_guard<std::mutex> lock(quizMutex);
            quiz.answers[subId][q->questionId] = answerIndex;

            std::cout << "[SERVER] Sub " << subId
                << " answered Q" << q->questionId
                << " with " << answerIndex << "\n";
        }

        q = q->next;
    }

    sendMsg(sock, MsgType::QUIZ_END, nullptr, 0);
}
*/
void startQuiz(Quiz& q) {
    q.status = QUIZ_RUNNING;
    std::cout << "[SERVER] Quiz " << q.quizId << " STARTED\n";

    SubNode* sub = q.subscribers;
    while (sub) {
        sendMsg(sub->sub.socket, MsgType::QUIZ_START, nullptr, 0);
        sub = sub->next;
    }
}
// ==================== Timer thread ====================

void quizTimerThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::lock_guard<std::mutex> lock(quizMutex);
        QuizNode* curr = allQuizzes;
        while (curr) {
            Quiz& q = curr->quiz;
            if (q.status == QUIZ_OPEN && time(nullptr) > q.registrationDeadline) {
                startQuiz(q);
            }
            curr = curr->next;
        }
    }
}
void sendAnswerToService(int subscriberId, int quizId, int questionId, int answerIndex) {
    std::lock_guard<std::mutex> lock(serviceSockMutex);
    if (g_serviceSock == INVALID_SOCKET) return;

    // Formiramo payload: "subscriberId|quizId|questionId|answerIndex"
    std::stringstream ss;
    ss << subscriberId << "|" << quizId << "|" << questionId << "|" << answerIndex;
    std::string msg = ss.str();

    sendMsg(g_serviceSock, MsgType::QUIZ_ANSWER, msg.c_str(), (uint32_t)msg.size());
}
void sendCorrectAnswerToService(int quizId, int questionId, int correctAnswer) {
    std::lock_guard<std::mutex> lock(serviceSockMutex);
    if (g_serviceSock == INVALID_SOCKET) return;

    // Formiramo payload: "quizId|questionId|answerIndex"
    std::stringstream ss;
    ss << quizId << "|" << questionId << "|" << correctAnswer;
    std::string msg = ss.str();

    sendMsg(g_serviceSock, MsgType::CORRECT_ANSWER, msg.c_str(), (uint32_t)msg.size());
}
void sendQuizListsToSubscriber(SOCKET clientSock) {
    std::lock_guard<std::mutex> lock(subSockMutex);

    // Formiramo payload: "quizId|questionId|answerIndex"
    QuizNode* curr = allQuizzes;
    std::stringstream ss;
    while (curr) {
        ss << curr->quiz.quizId << "|" << curr->quiz.topic << "\n";
        curr = curr->next;

    }
    std::string msg = ss.str();

    sendMsg(clientSock, MsgType::QUIZ_LIST, msg.c_str(), (uint32_t)msg.size());
}
// ==================== Client handler ====================

void handleSubscriber(SOCKET clientSock) {
    sendQuizListsToSubscriber(clientSock);
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;
    int subscriberId = -1;
    int quizId = -1;

    Quiz* activeQuiz = nullptr;
    Question* currentQuestion = nullptr;
    bool quizRunning = false;

    while (true) {
        if (!recvMsg(clientSock, type, payload, sizeof(payload) - 1, len)) {
            std::cout << "[SERVER] Subscriber disconnected\n";
            break;
        }

        payload[len] = '\0';

        switch (type) {

        case MsgType::PING:
            sendMsg(clientSock, MsgType::PONG, nullptr, 0);
            break;

            // ================= REGISTER =================
        case MsgType::REGISTER: {
            if (sscanf_s(payload, "%d %d", &quizId, &subscriberId) == 2) {
                bool ok = registerSubscriber(quizId, subscriberId, clientSock);
                sendMsg(clientSock,
                    ok ? MsgType::REGISTER_OK : MsgType::REGISTER_DENIED,
                    ok ? "OK" : "DENIED",
                    ok ? 2 : 6);

                if (ok) {
                    std::lock_guard<std::mutex> lock(quizMutex);
                    activeQuiz = findQuizById_NoLock(quizId);
                    std::cout << activeQuiz->topic << std::endl;
                }
            }
            else {
                sendMsg(clientSock, MsgType::REGISTER_DENIED, "BAD_FORMAT", 10);
            }
            break;
        }

                              // ================= QUIZ START =================
        case MsgType::QUIZ_START: {
            if (!activeQuiz) break;

            quizRunning = true;
            currentQuestion = activeQuiz->questions;
            std::cout << currentQuestion << std::endl;
            if (currentQuestion) {
                std::stringstream ss;
                ss << activeQuiz->quizId << "|"
                    << currentQuestion->questionId << "|"
                    << currentQuestion->text << "|"
                    << currentQuestion->options[0] << "|"
                    << currentQuestion->options[1] << "|"
                    << currentQuestion->options[2] << "|"
                    << currentQuestion->options[3];

                std::string msg = ss.str();
                sendMsg(clientSock, MsgType::QUIZ_QUESTION,
                    msg.c_str(), (uint32_t)msg.size());
            }
            break;
        }

                                // ================= ANSWER =================
        case MsgType::QUIZ_ANSWER: {
            if (!quizRunning || !currentQuestion) break;

            int answerIndex = atoi(payload);

            {
                std::lock_guard<std::mutex> lock(quizMutex);
                activeQuiz->answers[subscriberId]
                    [currentQuestion->questionId] = answerIndex;
            }
            sendAnswerToService(subscriberId, activeQuiz->quizId, currentQuestion->questionId, answerIndex);

            std::cout << "[SERVER] Sub " << subscriberId
                << " answered Q" << currentQuestion->questionId
                << " with " << answerIndex << "\n";

            currentQuestion = currentQuestion->next;

            if (currentQuestion) {
                std::stringstream ss;
                ss << activeQuiz->quizId << "|"
                    << currentQuestion->questionId << "|"
                    << currentQuestion->text << "|"
                    << currentQuestion->options[0] << "|"
                    << currentQuestion->options[1] << "|"
                    << currentQuestion->options[2] << "|"
                    << currentQuestion->options[3];

                std::string msg = ss.str();
                sendMsg(clientSock, MsgType::QUIZ_QUESTION,
                    msg.c_str(), (uint32_t)msg.size());
            }
            else {
                sendMsg(clientSock, MsgType::QUIZ_END, nullptr, 0);
                quizRunning = false;
            }
            break;
        }

                                 // ================= DISCONNECT =================
        case MsgType::DISCONNECT:
            std::cout << "[SERVER] Subscriber requested disconnect\n";
            goto cleanup;

        default:
            std::cout << "[SERVER] Unknown MsgType: "
                << (uint16_t)type << "\n";
        }
    }

cleanup:
    closesocket(clientSock);
    std::cout << "[SERVER] Subscriber closed socket\n";
}

void handlePublisher(SOCKET pubSock) {
    bool keep = true;
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;

    while (keep) {
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
            else {
                sendMsg(pubSock, MsgType::CREATE_QUIZ_ACK, "BAD_FORMAT", 10);
            }
            break;
        }

        case MsgType::ADD_QUESTION: {
            int quizId, qId, correct;
            char text[256]{}, o1[128]{}, o2[128]{}, o3[128]{}, o4[128]{};

            std::string payloadStr(payload, len); // pretvori u std::string
            std::vector<std::string> parts;
            std::stringstream ss(payloadStr);
            std::string item;

            while (std::getline(ss, item, '|')) {
                parts.push_back(item);
            }

            if (parts.size() == 8) {
                int quizId = std::stoi(parts[0]);
                int qId = std::stoi(parts[1]);
                int correct = std::stoi(parts[7]);

                Question q;
                q.questionId = qId;
                q.text = parts[2];
                q.options[0] = parts[3];
                q.options[1] = parts[4];
                q.options[2] = parts[5];
                q.options[3] = parts[6];
                q.correctOption = correct;

                addQuestionToQuiz(quizId, q);
                sendMsg(pubSock, MsgType::ADD_QUESTION_ACK, "OK", 2);
                sendCorrectAnswerToService(quizId, qId, correct);
            }
            else {
                sendMsg(pubSock, MsgType::ADD_QUESTION_ACK, "BAD_FORMAT", 10);
            }

            break;
        }


        case MsgType::DISCONNECT:
            keep = false;
            break;

        default:
            std::cout << "[SERVER] Unknown MsgType from publisher: " << (uint16_t)type << "\n";
        }
    }

    closesocket(pubSock);
    std::cout << "[SERVER] Publisher disconnected\n";
}
// ==================== MAIN ====================

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // Pokreni thread koji prati kvizove i startuje ih kad registracija istekne
    std::thread(quizTimerThread).detach();
    // 3️⃣ Service socket (6000)
    SOCKET listenService = createListenSocket(6000);
    if (listenService == INVALID_SOCKET) return 1;

    std::cout << "Server listening...\n";

    // Prihvati servis
    SOCKET serviceSock = accept(listenService, nullptr, nullptr);
    if (serviceSock == INVALID_SOCKET) return 1;
    {
        std::lock_guard<std::mutex> lk(serviceSockMutex);
        g_serviceSock = serviceSock;
    }
    sendMsg(serviceSock, MsgType::PING, nullptr, 0);
    std::cout << "Service connected\n";

    // 1️⃣ Publisher socket (poseban port, npr 5001)
    SOCKET listenPublisher = createListenSocket(5001);
    if (listenPublisher == INVALID_SOCKET) return 1;

    std::thread([&]() {
        while (true) {
            SOCKET pubSock = accept(listenPublisher, nullptr, nullptr);
            if (pubSock != INVALID_SOCKET) {
                // **ovdje mora handlePublisher**, ne handleSubscriber
                std::thread(handlePublisher, pubSock).detach();
            }
        }
        }).detach();

    // 2️⃣ Subscriber socket (5000)
    SOCKET listenSub = createListenSocket(5000);
    if (listenSub == INVALID_SOCKET) return 1;


    // Subscriber handler loop
    while (true) {
        SOCKET clientSock = accept(listenSub, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) break;
        std::thread(handleSubscriber, clientSock).detach();
    }

    std::cout << "Press ENTER to exit...\n";
    std::cin.get();
    closesocket(listenSub);
    closesocket(listenService);
    closesocket(serviceSock);
    closesocket(listenPublisher);
    WSACleanup();
    return 0;
}
