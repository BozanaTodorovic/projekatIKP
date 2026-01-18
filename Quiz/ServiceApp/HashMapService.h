#pragma once
#include <iostream>

#include "CorrectAnswerStruct.h"
struct SubResult {
    int subscriberId;
    int score;
    SubResult* next;
};

class HashMapService {
public:
    SubResult** buckets;
    size_t capacity;

    size_t hash(int subscriberId);

    HashMapService(size_t cap);
    ~HashMapService();

    void addOrUpdate(int subscriberId, int points);
    void printAll();
};

struct QuizResultNode {
    int quizId;
    HashMapService* subResults;
    RingBuffer* correctAnswers;
    QuizResultNode* next;
};

