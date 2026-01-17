#pragma once
#include <cstdint>

enum class MsgType : uint16_t {
    PING = 1,
    PONG = 2,

    CREATE_QUIZ = 10,
    CREATE_QUIZ_ACK = 11,
    ADD_QUESTION = 12,
    ADD_QUESTION_ACK = 13,

    REGISTER = 20,
    REGISTER_OK = 21,
    REGISTER_DENIED = 22,

    QUIZ_START = 30,
    QUIZ_QUESTION = 31,
    QUIZ_ANSWER = 32,
    QUIZ_RESULTS = 33,
    QUIZ_END = 34,
    CORRECT_ANSWER = 35,
    QUIZ_LIST = 36,
    QUIZ_RESULT=37,
    QUIZ_WAIT_RESULT = 38,
    QUIZ_TIME_UP=39,

    DISCONNECT = 40,

};
