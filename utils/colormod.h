#ifndef COLORMOD_H
#define COLORMOD_H

#include <ostream>

namespace Color {
    enum Code {
        FG_RED = 31,
        FG_GREEN = 32,
        FG_BLUE = 34,
        FG_DEFAULT = 39,
        BG_RED = 41,
        BG_GREEN = 42,
        BG_BLUE = 44,
        BG_DEFAULT = 49,
        FG_DARK_GRAY = 90,
        FG_LIGHT_RED = 91,
        FG_LIGHT_GREEN = 92,
        FG_LIGHT_YELLOW = 93, 
        FG_LIGHT_BLUE = 94, 
        FG_LIGHT_MAGENTA = 95, 
        FG_LIGHT_CYAN = 96,
    };
    class Modifier {
        Code code;
    public:
        Modifier(Code pCode) : code(pCode) {}
        friend std::ostream&
            operator<<(std::ostream& os, const Modifier& mod) {
            return os << "\033[" << mod.code << "m";
        }
    };
}

#endif // COLORMOD_H