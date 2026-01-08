#pragma once
#include "QuizStruct.h"

#define HASH_SIZE 256

struct HashNode {
    int key;
    Subscriber value;
    HashNode* next;
};

class HashMap {
public:
    HashNode* buckets[HASH_SIZE];

    void init();
    int hash(int key);
    void insert(int key, const Subscriber& val);
    Subscriber* find(int key);
};
