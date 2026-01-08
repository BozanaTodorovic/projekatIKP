#include "SubscriberUtils.h"

void splitQuizMessage(const char* msg, char parts[7][256]) {
    int partIndex = 0;
    int charIndex = 0;

    for (int i = 0; msg[i] != '\0'; i++) {
        if (msg[i] == '|') {
            parts[partIndex][charIndex] = '\0';
            partIndex++;
            charIndex = 0;
            if (partIndex >= 7) break; //have 7 parts: quizId, qId, text, 4 questions
        }
        else {
            parts[partIndex][charIndex++] = msg[i];
            if (charIndex >= 255) charIndex = 255; //  overflow
        }
    }
    parts[partIndex][charIndex] = '\0';
}