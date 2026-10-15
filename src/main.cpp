#include <cstdio>
#include "parser.h"

int main() {
    Parser parser;

    fprintf(stderr, "ready> ");
    parser.getNextToken();

    while (true) {
        fprintf(stderr, "ready> ");
        switch (parser.currToken) {
            case tok_eof:
                return 0;
            case ';': // ignore top-level semicolons.
                parser.getNextToken();
                break;
            case tok_def:
                parser.HandleDefinition();
                break;
            case tok_extern:
                parser.HandleExtern();
                break;
            default:
                parser.HandleTopLevelExpression();
                break;
        }
    }
}
