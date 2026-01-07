#include "Utils.h"

void setQuizes(Quizes array[3]) {
    Quizes quiz1 = {
    "OpsteZnanje", // topic
    {   // questions[]
        {1, "Koji je glavni grad Srbije?", {"Beograd","Nis","Novi Sad","Kragujevac"}, 1},
        {2, "Koliko planeta ima Suncev sistem?", {"7","8","9","10"}, 2},
        {3, "Koja boja nastaje mesanjem plave i zute?", {"Crvena","Zelena","Plava","Zuta"}, 2}
    }
    };

    Quizes quiz2 = {
        "Geografija",
        {
            {1, "Koji je glavni grad Madjarske?", {"Budimpesta","Nis","Novi Sad","Kragujevac"}, 1},
            {2, "Koja reka protice kroz Pariz?", {"Seina","Dunav","Volga","Nil"}, 1},
            {3, "Najveca pustinja na svetu?", {"Sahara","Gobi","Kalahari","Arabijska"}, 1}
        }
    };

    Quizes quiz3 = {
        "Razno",
        {
            {1, "Koji je glavni grad Makedonije?", {"Skoplje","Nis","Beograd","Sofia"}, 1},
            {2, "Koja boja nastaje mesanjem crvene i zute?", {"Narandzasta","Zelena","Plava","Zuta"}, 1},
            {3, "Koja zivotinja je najveca na svetu?", {"Plavi kit","Slon","Zirafa","Kit ubica"}, 2}
        }
    };
    array[0] = quiz1;
    array[1] = quiz2;
    array[2] = quiz3;
}

bool sendQuestion(SOCKET sock, int quizId, const QuestionToSend& q) {
    char payload[1024]{};

    sprintf_s(payload, sizeof(payload),
        "%d|%d|%s|%s|%s|%s|%s|%d",
        quizId,
        q.questionId,
        q.text,
        q.options[0],
        q.options[1],
        q.options[2],
        q.options[3],
        q.correctOption
    );

    if (!sendMsg(sock, MsgType::ADD_QUESTION,
        payload, (uint32_t)strlen(payload))) {
        std::cout << "Failed to send ADD_QUESTION\n";
        return false;
    }

    MsgType type;
    char buf[256]{};
    uint32_t len = 0;

    if (recvMsg(sock, type, buf, sizeof(buf) - 1, len)) {
        buf[len] = '\0';
        return (type == MsgType::ADD_QUESTION_ACK);
    }

    return false;
}