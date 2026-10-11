#pragma once

#include <string>

// All controller button captions live here so a future controller style can replace the
// Xbox labels with another pad's symbols in one place.
extern std::string kButtonLabelA;
extern std::string kButtonLabelB;
extern std::string kButtonLabelX;
extern std::string kButtonLabelY;
extern std::string kButtonLabelStart;
extern std::string kButtonLabelSelect;
extern std::string kButtonLabelL1;
extern std::string kButtonLabelL2;
extern std::string kButtonLabelR1;
extern std::string kButtonLabelR2;

// Change these two characters to switch the display style globally, for example to [A] or {A}.
extern char kButtonLabelPreChar;
extern char kButtonLabelPostChar;

void setFaceButtonStyle(const std::string &style);
std::string buttonLabel(const std::string &button);
std::string buttonName(const std::string &button);
std::string formatButtonLabels(const std::string &text);
