#include "ButtonLabels.h"

#include <cctype>
#include <vector>

std::string kButtonLabelA = "A";
std::string kButtonLabelB = "B";
std::string kButtonLabelX = "X";
std::string kButtonLabelY = "Y";
std::string kButtonLabelStart = "START";
std::string kButtonLabelSelect = "SELECT";
std::string kButtonLabelL1 = "LB";
std::string kButtonLabelL2 = "LT";
std::string kButtonLabelR1 = "RB";
std::string kButtonLabelR2 = "RT";

char kButtonLabelPreChar = '(';
char kButtonLabelPostChar = ')';

namespace {

struct ButtonToken {
    const char *source;
    const std::string *label;
};

const ButtonToken kTokens[] = {
    {"A", &kButtonLabelA},       {"B", &kButtonLabelB},
    {"X", &kButtonLabelX},       {"Y", &kButtonLabelY},
    {"START", &kButtonLabelStart}, {"SELECT", &kButtonLabelSelect},
    {"L1", &kButtonLabelL1},     {"L2", &kButtonLabelL2},
    {"R1", &kButtonLabelR1},     {"R2", &kButtonLabelR2},
    {"LB", &kButtonLabelL1},     {"LT", &kButtonLabelL2},
    {"RB", &kButtonLabelR1},     {"RT", &kButtonLabelR2},
};

bool wordChar(unsigned char ch) { return std::isalnum(ch) || ch == '_'; }

std::string upper(const std::string &text) {
    std::string result = text;
    for (char &ch : result) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return result;
}

bool isInputStart(const std::string &text, size_t begin, size_t end,
                  const std::string &token) {
    if (token != "START" && token != "SELECT") return true;

    // In a hint such as "A Start" or "A Select", the word after the face button is an
    // action description. The physical Start/Select button is listed on its own instead.
    size_t previous = begin;
    while (previous > 0 && std::isspace(static_cast<unsigned char>(text[previous - 1])))
        --previous;
    if (previous > 0 && (text[previous - 1] == 'A' || text[previous - 1] == 'B' ||
                         text[previous - 1] == 'X' || text[previous - 1] == 'Y')) {
        const size_t faceBegin = previous - 1;
        if (faceBegin == 0 || !wordChar(static_cast<unsigned char>(text[faceBegin - 1])))
            return false;
    }

    (void)end;
    return true;
}

} // namespace

void setFaceButtonStyle(const std::string &style) {
    const std::string key = upper(style);
    if (key == "PLAYSTATION") {
        kButtonLabelA = "✕"; // bottom / confirm
        kButtonLabelB = "○"; // right / back
        kButtonLabelX = "□"; // left
        kButtonLabelY = "△"; // top
        kButtonLabelL1 = "L1";
        kButtonLabelL2 = "L2";
        kButtonLabelR1 = "R1";
        kButtonLabelR2 = "R2";
    } else if (key == "NINTENDO") {
        kButtonLabelA = "B"; // bottom / confirm
        kButtonLabelB = "A"; // right / back
        kButtonLabelX = "Y"; // left
        kButtonLabelY = "X"; // top
        kButtonLabelL1 = "L";
        kButtonLabelL2 = "ZL";
        kButtonLabelR1 = "R";
        kButtonLabelR2 = "ZR";
    } else {
        kButtonLabelA = "A";
        kButtonLabelB = "B";
        kButtonLabelX = "X";
        kButtonLabelY = "Y";
        kButtonLabelL1 = "LB";
        kButtonLabelL2 = "LT";
        kButtonLabelR1 = "RB";
        kButtonLabelR2 = "RT";
    }
}

std::string buttonLabel(const std::string &button) {
    const std::string key = upper(button);
    for (const ButtonToken &token : kTokens) {
        if (key == token.source)
            return std::string(1, kButtonLabelPreChar) + *token.label + kButtonLabelPostChar;
    }
    return button;
}

std::string buttonName(const std::string &button) {
    const std::string key = upper(button);
    for (const ButtonToken &token : kTokens) {
        if (key == token.source)
            return *token.label;
    }
    return button;
}

std::string formatButtonLabels(const std::string &text) {
    std::string result;
    result.reserve(text.size() + 24);

    for (size_t i = 0; i < text.size();) {
        if (i > 0 && wordChar(static_cast<unsigned char>(text[i - 1]))) {
            result.push_back(text[i++]);
            continue;
        }

        size_t end = i;
        while (end < text.size() && wordChar(static_cast<unsigned char>(text[end]))) ++end;
        if (end == i) {
            result.push_back(text[i++]);
            continue;
        }

        const std::string source = text.substr(i, end - i);
        const std::string key = upper(source);
        const bool alreadyWrapped = i > 0 && end < text.size() &&
                                    text[i - 1] == kButtonLabelPreChar &&
                                    text[end] == kButtonLabelPostChar;
        bool replaced = false;
        for (const ButtonToken &token : kTokens) {
            if (key != token.source || !isInputStart(text, i, end, key)) continue;
            if (alreadyWrapped) result += *token.label;
            else result += std::string(1, kButtonLabelPreChar) + *token.label +
                           kButtonLabelPostChar;
            replaced = true;
            break;
        }
        if (!replaced) result += source;
        i = end;
    }
    return result;
}
