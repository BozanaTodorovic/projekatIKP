#include "Handlers.h"
#include <iostream>
#include <chrono>

HashMap subscriberMap;
SOCKET g_serviceSock = INVALID_SOCKET;
std::mutex serviceSockMutex;
QuizBuffer allQuizzes;
std::mutex quizMutex;
static int quizId = 1;
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

    std::cout << "\n[SERVER] Quiz " << quizId << " created\n";
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

    std::cout << "\n\n[SERVER] Subscriber " << subId << " registered to quiz " << quizId << "\n";
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
    std::cout <<"\n[SERVER] Quiz " << q.quizId << " STARTED";
    for (int i = 0; i < q.subscriberCount; i++) {
        sendMsg(q.subscribers[i].sock, MsgType::QUIZ_START, nullptr, 0);
    }
}
void endQuiz(Quiz& q) {
    q.status = QUIZ_FINISHED;
    std::cout << "[SERVER] Quiz " << q.quizId << " ENDED\n\n" << std::endl;

    for (int i = 0; i < q.subscriberCount; i++) {
        sendMsg(q.subscribers[i].sock, MsgType::QUIZ_WAIT_RESULT, nullptr, 0);
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
            if (q.status == QUIZ_OPEN && time(nullptr) > q.registrationDeadline && q.subscriberCount > 0) {
                q.quizEndTime = time(nullptr) + q.quizDurationSeconds;
                startQuiz(q);
            }
        }
    }
}

void quizEndTimerThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::lock_guard<std::mutex> lock(quizMutex);
        for (int i = 0; i < allQuizzes.count; i++) {
            int idx = (allQuizzes.head + i) % MAX_QUIZZES;
            Quiz& q = allQuizzes.buffer[idx];
            //std::cout << "Broj subscribera za kviz "<<q.quizId <<" je " << q.subscriberCount << std::endl;
            if (q.status == QUIZ_RUNNING && q.subscriberCount>0  && time(nullptr) > q.quizEndTime) {
                endQuiz(q);
                sendQuizEndToService(q.quizId);
            }
        }
    }
}

void handleSubscriber(SOCKET clientSock) {
    MsgType type;
    char payload[1024]{};
    uint32_t len = 0;
    int subscriberId = -1;
    int quizId = -1;
    Quiz* activeQuiz = nullptr;
    int currentQuestionIdx = 0;
    time_t quizDurationStart;
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
            std::cout << "\n[SERVER] Subscriber disconnected\n";
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
               /* quizDurationStart = std::time(nullptr); // Get current time as a time_t value
                char timeStr[26];   // ctime_s zahtijeva buffer od bar 26 chara

                ctime_s(timeStr, sizeof(timeStr), &quizDurationStart);
                std::cout << timeStr;
                quizDurationStart += 30; // ctime_s zahtijeva buffer od bar 26 chara

                ctime_s(timeStr, sizeof(timeStr), &quizDurationStart);
                std::cout << timeStr;*/
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
                if (currentQuestionIdx < MAX_QUESTIONS)  // zastita od out-of-bounds
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
            int  regSec, durSec;
            char topic[128]{};
            if (sscanf_s(payload, "%d %d %127s", &regSec, &durSec, topic, (unsigned)_countof(topic)) >= 3) {
                createQuiz(quizId, regSec, durSec, topic);
                char msg[128];
                snprintf(msg, sizeof(msg), "%d", quizId);
                sendMsg(pubSock, MsgType::CREATE_QUIZ_ACK, msg, (uint32_t)strlen(msg));
                quizId++;
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
            Subscriber* sub = subscriberMap.find(subId); // trebas funkciju koja trazi subscriber
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
