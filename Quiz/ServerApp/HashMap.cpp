#include "HashMap.h"
#include <cstring>

void HashMap::init() {
    memset(buckets, 0, sizeof(buckets));
}

int HashMap::hash(int key) {
    return key % HASH_SIZE;
}

void HashMap::insert(int key, const Subscriber& val) {
    int h = hash(key);
    buckets[h] = new HashNode{ key, val, buckets[h] };
}

Subscriber* HashMap::find(int key) {
    int h = hash(key);
    HashNode* node = buckets[h];
    while (node) {
        if (node->key == key) return &node->value;
        node = node->next;
    }
    return nullptr;
}
