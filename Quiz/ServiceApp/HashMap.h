#pragma once
#include <iostream>

#include "CorrectAnswerStruct.h"
struct SubResult {
    int subscriberId;
    int score;
    SubResult* next;
};

class HashMap {
public:
    SubResult** buckets;
    size_t capacity;

    size_t hash(int subscriberId);

    HashMap(size_t cap);
    ~HashMap();

    void addOrUpdate(int subscriberId, int points);
    void printAll();
};

struct QuizResultNode {
    int quizId;
    HashMap* subResults;
    RingBuffer* correctAnswers;
    QuizResultNode* next;
};

