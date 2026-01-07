#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include "../Common/protocol.h"
#include "../Common/net.h"
#include "HashMap.h"

QuizResultNode* getOrCreateQuiz(int quizId);
void addCorrectAnswer(const char* payload);
void processQuizAnswer(int quizId, const char* payload);
void sendQuizResult(SOCKET sock, int quizId);
