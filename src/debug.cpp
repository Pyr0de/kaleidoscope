#include "debug.h"
#include "ast.h"
#include <memory>

std::unique_ptr<llvm::DIBuilder> DBuilder;
DebugInfo KSDbgInfo;

void InitializeDebug() {
    KSDbgInfo = DebugInfo();
    DBuilder = std::make_unique<llvm::DIBuilder>(*TheModule);

    KSDbgInfo.TheCU = DBuilder->createCompileUnit(
            llvm::dwarf::DW_LANG_C,
            DBuilder->createFile("fib.ks", "."),
            "Kaleidoscope Compiler",
            false,
            "",
            0
            );
}

DISubroutineType* CreateFunctionType(unsigned NumArgs) {
    SmallVector<Metadata *, 8> EltTys;
    DIType *DblTy = KSDbgInfo.getDoubleTy();

    // Add the result type.
    EltTys.push_back(DblTy);

    for (unsigned i = 0, e = NumArgs; i != e; ++i)
        EltTys.push_back(DblTy);

    return DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(EltTys));
}

void DebugInfo::emitLocation(ExprAST *AST) {
    if (!AST)
        return Builder->SetCurrentDebugLocation(DebugLoc());
    DIScope *Scope;
    if (LexicalBlocks.empty())
        Scope = TheCU;
    else
        Scope = LexicalBlocks.back();
    Builder->SetCurrentDebugLocation(DILocation::get(
                Scope->getContext(),
                AST->getLine(),
                AST->getCol(),
                Scope
                ));
}
