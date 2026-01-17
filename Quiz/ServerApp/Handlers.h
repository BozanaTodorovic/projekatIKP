#pragma once
#include <winsock2.h>
#include <mutex>
#include "QuizStruct.h"
#include "HashMap.h"
#include "../Common/net.h"
#include "../Common/protocol.h"

extern HashMap subscriberMap;
extern SOCKET g_serviceSock;
extern std::mutex serviceSockMutex;
extern QuizBuffer allQuizzes;
extern std::mutex quizMutex;

// FUNKCIJE
void sendAnswerToService(int subscriberId, int quizId, int questionId, int answerIndex);
void sendQuizEndToService(int quizId);
void sendCorrectAnswerToService(int quizId, int questionId, int correctAnswer);
void createQuiz(int quizId, int regDurationSec, int quizDurationSec, const char* topic);
bool registerSubscriber(int quizId, int subId, SOCKET sock);
bool addQuestionToQuiz(int quizId, const Question& q);
void endQuiz(Quiz& q);
void startQuiz(Quiz& q);
void quizTimerThread();
void quizEndTimerThread();
void handleSubscriber(SOCKET clientSock);
void handlePublisher(SOCKET pubSock);
void handleService(SOCKET serviceSock);

