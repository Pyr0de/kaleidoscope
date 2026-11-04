#pragma once

#include "ast.h"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>

struct DebugInfo;

extern std::unique_ptr<llvm::DIBuilder> DBuilder;
extern DebugInfo KSDbgInfo;

void InitializeDebug();
llvm::DISubroutineType *CreateFunctionType(unsigned NumArgs);

struct DebugInfo {
    llvm::DICompileUnit* TheCU;
    llvm::DIType* DblTy;
    std::vector<DIScope*> LexicalBlocks;

    llvm::DIType* getDoubleTy() {
        if (DblTy)
            return DblTy;

        DblTy = DBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
        return DblTy;
    }
    void emitLocation(ExprAST *AST);
};
