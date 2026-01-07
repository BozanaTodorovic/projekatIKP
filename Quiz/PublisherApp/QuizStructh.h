#pragma once

#define QUESTIONS_COUNT 3
struct QuestionToSend {
    int questionId;
    char text[256];
    char options[4][128];
    int correctOption;
};

struct Quizes {
    char topic[127];
    QuestionToSend questions[QUESTIONS_COUNT];
};
