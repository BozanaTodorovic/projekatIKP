#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include "QuizStructh.h"
#include "../Common/net.h"

void setQuizes(Quizes array[QUESTIONS_COUNT]);
bool sendQuestion(SOCKET sock, int quizId, const QuestionToSend& q);