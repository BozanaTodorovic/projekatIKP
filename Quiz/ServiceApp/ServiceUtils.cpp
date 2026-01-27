#include "ServiceUtils.h"
QuizResultNode* allQuizzes = nullptr;

QuizResultNode* getOrCreateQuiz(int quizId) {
    QuizResultNode* curr = allQuizzes;
    while (curr) { if (curr->quizId == quizId) return curr; curr = curr->next; }
    QuizResultNode* newQuiz = new QuizResultNode;
    newQuiz->quizId = quizId;
    newQuiz->subResults = new HashMapService(100);
    newQuiz->correctAnswers = new RingBuffer(100);
    newQuiz->next = allQuizzes;
    allQuizzes = newQuiz;
    return newQuiz;
}

void addCorrectAnswer(const char* payload) {
    int quizId = 0, qId = 0, correctAnswer = 0;

    if (sscanf_s(payload, "%d|%d|%d",
        &quizId, &qId, &correctAnswer) != 3)
        return;

    QuizResultNode* quiz = getOrCreateQuiz(quizId);
    quiz->correctAnswers->push({ qId, correctAnswer });

   //std::cout << "[SERVICE] Correct answer loaded quiz="<< quizId << " q=" << qId<< " correct=" << correctAnswer << "\n";
}


void processQuizAnswer(int quizId, const char* payload) {
    int subId = 0, qId = 0, answer = 0, qz = 0;

    // payload: subId|quizId|questionId|answer
    if (sscanf_s(payload, "%d|%d|%d|%d",
        &subId, &qz, &qId, &answer) != 4)
        return;

    QuizResultNode* quiz = getOrCreateQuiz(quizId);

    int correctOption = -1;
    int points = 0;

    if (quiz->correctAnswers->get(qId, correctOption)) {
        if (answer == correctOption)
            points = 2;
    }

    // SABIRANJE bodova
    quiz->subResults->addOrUpdate(subId, points);

    std::cout << "[SERVICE] quiz=" << quizId<< " sub=" << subId<< " q=" << qId<< " ans=" << answer<< " correct=" << correctOption<< " +" << points<< "\n";

    quiz->subResults->printAll();
}

void sendQuizResult(SOCKET sock, int quizId) {
    QuizResultNode* quiz = allQuizzes;
    while (quiz) {
        if (quiz->quizId == quizId) break;
        quiz = quiz->next;
    }
    if (!quiz) return; // nema kviza

    char msg[128];

    for (size_t i = 0; i < quiz->subResults->capacity; i++) {
        SubResult* curr = quiz->subResults->buckets[i];
        while (curr) {
            // subscriberId|quizId|score
            snprintf(msg, sizeof(msg), "%d|%d|%d", curr->subscriberId, quizId, curr->score);
            sendMsg(sock, MsgType::QUIZ_RESULT, msg, (uint32_t)strlen(msg));
            curr = curr->next;
        }
    }
}