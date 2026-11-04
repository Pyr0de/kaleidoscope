#include <cstdio>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Triple.h>
#include <string>
#include "parser.h"
#include "ast.h"

int mainLoop() {
    Parser parser;

    while (true) {
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
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    std::string error;
    auto targetTriple = sys::getDefaultTargetTriple();

    llvm::Triple triple(targetTriple);

    auto target = llvm::TargetRegistry::lookupTarget(triple, error);

    auto CPU = "generic";
    auto features = "";

    TargetOptions opt;
    auto TargetMachine = target->createTargetMachine(triple, CPU, features, opt, Reloc::PIC_);

    // JIT = ExitOnErr(orc::KaleidoscopeJIT::Create());

    InitializeModuleAndManagers(TargetMachine, triple);

    mainLoop();

    auto Filename = "output.o";
    std::error_code EC;
    raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

    if (EC) {
        errs() << "Could not open file: " << EC.message();
        return 1;
    }

    legacy::PassManager pass;
    auto FileType = CodeGenFileType::ObjectFile;

    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        errs() << "TheTargetMachine can't emit a file of this type";
        return 1;
    }

    pass.run(*TheModule);
    dest.flush();

    outs() << "Wrote " << Filename << "\n";

    return 0;
}
