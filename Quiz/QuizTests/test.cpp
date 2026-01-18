#include "pch.h"
#include "SubscriberUtils.h"
#include "Utils.h"
#include "Handlers.h"
#include "ServiceUtils.h"
#include <cstring>
#include <thread>

//sub se na vrijeme prijavio
TEST(ServerUnitTests, Register_Subscriber_OK) {
    int quizId = 1;int subId = 2; SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Quiz q{};
    q.quizId = quizId;
    strncpy_s(q.topic, "Topic 1", MAX_TOPIC_LEN - 1);
    q.topic[MAX_TOPIC_LEN - 1] = '\0';
    q.status = QUIZ_OPEN;
    q.registrationDeadline = time(nullptr) + 10;
    q.quizDurationSeconds = 20;
    q.questionCount = 0;
    q.subscriberCount = 0;

    allQuizzes.push(q);


    bool success=registerSubscriber(quizId, subId, sock);

    EXPECT_TRUE(success);
   
}
//kviz zapoceo,nije moguca registracija
TEST(ServerUnitTests, Register_Subscriber_False) {
    int quizId = 2;int subId = 2; SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Quiz q{};
    q.quizId = quizId;
    strncpy_s(q.topic, "Topic 1", MAX_TOPIC_LEN - 1);
    q.topic[MAX_TOPIC_LEN - 1] = '\0';
    q.status = QUIZ_RUNNING;
    q.registrationDeadline = time(nullptr) + 10;
    q.quizDurationSeconds = 20;
    q.questionCount = 0;
    q.subscriberCount = 0;

    allQuizzes.push(q);


    bool success=registerSubscriber(quizId, subId, sock);

    EXPECT_FALSE(success);
   
}

//dodato vise pitanja nego sto moze
TEST(ServerUnitTests, AddQuestion_False) {
    Quiz q{};
    q.quizId = 33;
    strncpy_s(q.topic, "Topic 1", MAX_TOPIC_LEN - 1);
    q.topic[MAX_TOPIC_LEN - 1] = '\0';
    q.status = QUIZ_OPEN;
    q.registrationDeadline = time(nullptr) + 10;
    q.quizDurationSeconds = 20;
    q.questionCount = 0;
    q.subscriberCount = 0;

    allQuizzes.push(q);
    Quiz* quiz = allQuizzes.findById(33);
    Question question{};
    for (int i = 0;i <4 ;i++) {
        question.questionId = i+1;
        strncpy_s(question.text, "Teks pitanja"+i, MAX_QUEST_LEN - 1);
        strncpy_s(question.options[0], "1", MAX_OPTION_LEN - 1);
        strncpy_s(question.options[1], "2", MAX_OPTION_LEN - 1);
        strncpy_s(question.options[2], "3", MAX_OPTION_LEN - 1);
        strncpy_s(question.options[3], "4", MAX_OPTION_LEN - 1);
        question.correctOption = 1;
        addQuestionToQuiz(33, question);
    }
    question.questionId = 5;
    strncpy_s(question.text, "Teks pitanja 5", MAX_QUEST_LEN - 1);
    strncpy_s(question.options[0], "1", MAX_OPTION_LEN - 1);
    strncpy_s(question.options[1], "2", MAX_OPTION_LEN - 1);
    strncpy_s(question.options[2], "3", MAX_OPTION_LEN - 1);
    strncpy_s(question.options[3], "4", MAX_OPTION_LEN - 1);
    question.correctOption = 1;

    bool success = addQuestionToQuiz(33, question);

    EXPECT_EQ(quiz->questionCount,4);
    EXPECT_FALSE(success);
}


//uspjesno dodati kvizovi
TEST(PublisherUnitTests, Create_Quiz_OK) {
    Quizes mojNiz[QUESTIONS_COUNT];

    setQuizes(mojNiz);

    EXPECT_EQ(sizeof(mojNiz) / sizeof(mojNiz[0]), 3);
    EXPECT_STREQ(mojNiz[0].topic, "OpsteZnanje");
    EXPECT_EQ(mojNiz[2].questions[2].correctOption, 2);
}


//socket nije povezan na server,samo je kreiran 
TEST(PublisherUnitTests,False_Sending_Question) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); int quizId = 1;
    QuestionToSend q;
    q.questionId = 3;
    strcpy_s(q.text, "Koja zivotinja je najveca na svetu?");
    strcpy_s(q.options[0], "Plavi kit");
    strcpy_s(q.options[1], "Slon");
    strcpy_s(q.options[2], "Magarac");
    strcpy_s(q.options[3], "Pas");
    q.correctOption = 1;

    bool success=sendQuestion(sock,quizId,q);

    EXPECT_FALSE(success);
    closesocket(sock);
    WSACleanup();
}
//soket povezan,validno slanje pitanja
TEST(SendQuestionLogic_True,ValidSendQuestion) {
    // 1. Inicijalizacija Winsock-a
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 2. Kreiranje parova povezanih socketa (Simulacija mrezne veze)
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // OS bira slobodan port

    bind(listener, (sockaddr*)&addr, sizeof(addr));
    listen(listener, 1);

    int addrLen = sizeof(addr);
    getsockname(listener, (sockaddr*)&addr, &addrLen);

    // Klijentski socket koji salje
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(clientSock, (sockaddr*)&addr, sizeof(addr));

    // Serverski socket koji prima i odgovara
    SOCKET serverSock = accept(listener, nullptr, nullptr);


    QuestionToSend q = { 3, "Pitanje?", {"A","B","C","D"}, 0 };


    std::thread serverThread([serverSock]() {
        MsgType type;
        char buf[1024];
        uint32_t len;


        if (recvMsg(serverSock, type, buf, sizeof(buf), len)) {
            sendMsg(serverSock, MsgType::ADD_QUESTION_ACK, "OK", 2);
        }
        });

    bool success = sendQuestion(clientSock, 1, q);

    EXPECT_TRUE(success);

    serverThread.join();
    closesocket(clientSock);
    closesocket(serverSock);
    closesocket(listener);
    WSACleanup();
}

// Testiramo da li funkcija ispravno deli poruku sa separatorom '|'
TEST(SubscriberUnitTests, SplitQuizMessage_ValidInput) {
    const char* msg = "Q1|1|Glavni grad Srbije?|Beograd|Novi Sad|Nis|Kragujevac";
    char parts[7][256];

    splitQuizMessage(msg, parts);

    EXPECT_STREQ(parts[0], "Q1");
    EXPECT_STREQ(parts[1], "1");
    EXPECT_STREQ(parts[2], "Glavni grad Srbije?");
    EXPECT_STREQ(parts[3], "Beograd");
    EXPECT_STREQ(parts[4], "Novi Sad");
    EXPECT_STREQ(parts[5], "Nis");
    EXPECT_STREQ(parts[6], "Kragujevac");
}

// Testiramo šta se dešava ako poruka ima manje delova
TEST(SubscriberUnitTests, SplitQuizMessage_PartialInput) {
    const char* msg = "Q2|202|Neko pitanje";
    char parts[7][256];

    //da budemo sigurni da je nesto ubaceno
    for (int i = 0; i < 7; i++) parts[i][0] = '\0';

    splitQuizMessage(msg, parts);

    EXPECT_STREQ(parts[0], "Q2");
    EXPECT_STREQ(parts[1], "202");
    EXPECT_STREQ(parts[2], "Neko pitanje");
}
//ako se posalje prazna poruka
TEST(ServiceUnitTests, AddCorrectAnswer_EmptyPayload) {
    const char* msg = "";

    QuizResultNode* result = getOrCreateQuiz(1);
    addCorrectAnswer(msg);

    EXPECT_EQ(result->correctAnswers->size, 0);

}

//da li se tacno pitanje ispravno doda u buffer
TEST(ServiceUnitTests, AddCorrectAnswer_Success) {
    const char* msg = "1|1|1";
    
    QuizResultNode* result = getOrCreateQuiz(1);
    addCorrectAnswer(msg);

    EXPECT_EQ(result->correctAnswers->buffer->questionId,1);
    EXPECT_NE(result->correctAnswers->size, 0);

}