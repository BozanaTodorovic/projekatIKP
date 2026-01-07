#pragma once
#include "HashMap.h"

// ===================== PRIVATE =====================
size_t HashMap::hash(int subscriberId) {
    return subscriberId % capacity;
}

// ===================== PUBLIC =====================
HashMap::HashMap(size_t cap) : capacity(cap) {
    buckets = new SubResult * [cap];
    for (size_t i = 0; i < cap; i++) buckets[i] = nullptr;
}

HashMap::~HashMap() {
    for (size_t i = 0; i < capacity; i++) {
        SubResult* curr = buckets[i];
        while (curr) {
            SubResult* tmp = curr;
            curr = curr->next;
            delete tmp;
        }
    }
    delete[] buckets;
}

void HashMap::addOrUpdate(int subscriberId, int points) {
    size_t idx = hash(subscriberId);
    SubResult* curr = buckets[idx];
    while (curr) {
        if (curr->subscriberId == subscriberId) {
            curr->score += points;
            return;
        }
        curr = curr->next;
    }
    SubResult* newSub = new SubResult{ subscriberId, points, buckets[idx] };
    buckets[idx] = newSub;
}

void HashMap::printAll() {
    for (size_t i = 0; i < capacity; i++) {
        SubResult* curr = buckets[i];
        while (curr) {
            std::cout << "Sub " << curr->subscriberId << " -> " << curr->score << " pts\n";
            curr = curr->next;
        }
    }
}

