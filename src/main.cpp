#include <cstdio>
#include "lexer.h"

int main() {
    Lexer lex;
    for (int t; (t = lex.getToken()) != tok_eof;)
        std::printf("token: %d\n", t);
}
