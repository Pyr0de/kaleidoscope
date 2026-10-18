#include <cstdio>
#include <llvm/Support/TargetSelect.h>
#include "KaleidoscopeJIT.h"
#include "parser.h"
#include "ast.h"

int mainLoop() {
    Parser parser;

    while (true) {
        fprintf(stderr, "ready > ");
        parser.getNextToken();
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


int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    JIT = ExitOnErr(orc::KaleidoscopeJIT::Create());

    InitializeModuleAndManagers();

    mainLoop();

    TheModule->print(errs(), nullptr);
}
