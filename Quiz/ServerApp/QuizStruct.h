#pragma once
#include <winsock2.h>
#include <ctime>

#define MAX_TOPIC_LEN 128
#define MAX_QUEST_LEN 256
#define MAX_OPTION_LEN 128
#define MAX_QUESTIONS 32
#define MAX_SUBSCRIBERS 128
#define MAX_QUIZZES 64

enum QuizStatus {
    QUIZ_OPEN,
    QUIZ_RUNNING,
    QUIZ_FINISHED
};

struct Question {
    int questionId;
    char text[MAX_QUEST_LEN];
    char options[4][MAX_OPTION_LEN];
    int correctOption;
};

struct Subscriber {
    int subscriberId;
    SOCKET sock;
    int answers[MAX_QUESTIONS];
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
struct QuizBuffer {
    Quiz buffer[MAX_QUIZZES];
    int head = 0;
    int tail = 0;
    int count = 0;

    void push(const Quiz& q) {
        buffer[tail] = q;
        tail = (tail + 1) % MAX_QUIZZES;
        if (count < MAX_QUIZZES) {
            count++;
        }
        else {
            head = (head + 1) % MAX_QUIZZES; // prepisuje najstariji
        }
    }

    Quiz* findById(int quizId) {
        for (int i = 0, idx = head; i < count; i++, idx = (idx + 1) % MAX_QUIZZES) {
            if (buffer[idx].quizId == quizId) {
                return &buffer[idx];
            }
        }
        return nullptr;
    }
};