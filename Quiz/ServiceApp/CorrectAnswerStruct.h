#pragma once
struct CorrectAnswer {
    int questionId;
    int correctOption;
};

struct RingBuffer {
    CorrectAnswer* buffer;
    size_t capacity;
    size_t start = 0;
    size_t size = 0;

    RingBuffer(size_t cap) : capacity(cap) { buffer = new CorrectAnswer[cap]; }
    ~RingBuffer() { delete[] buffer; }

    void push(const CorrectAnswer& ca) {
        buffer[(start + size) % capacity] = ca;
        if (size < capacity) size++;
        else start = (start + 1) % capacity;
    }

    bool get(int questionId, int& correctOption) {
        for (size_t i = 0; i < size; i++) {
            size_t idx = (start + i) % capacity;
            if (buffer[idx].questionId == questionId) {
                correctOption = buffer[idx].correctOption;
                return true;
            }
        }
        return false;
    }
};