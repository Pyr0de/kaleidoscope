#pragma once
#include <string>

struct SourceLocation {
    int Line;
    int Col;
};

extern SourceLocation CurLoc;
extern SourceLocation LexLoc;

enum Token {
    tok_eof = -1,

    tok_def = -2,
    tok_extern = -3,

    tok_ident = -4,
    tok_number = -5,

    tok_if = -6,
    tok_then = -7,
    tok_else = -8,

    tok_for = -9,
    tok_in = -10,

    tok_binary = -11,
    tok_unary = -12,

    tok_var = -13,
};

class Lexer {
    public:
        int getToken();
        const std::string &identifier() const { return IdentifierStr; }
        double number() const { return NumVal; }
    private:
        char LastChar = ' ';
        double NumVal = 0;
        std::string IdentifierStr;
};
