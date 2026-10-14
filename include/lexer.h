#pragma once
#include <string>

enum Token {
    tok_eof = -1,

    tok_def = -2,
    tok_extern = -3,

    tok_ident = -4,
    tok_number = -5
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
