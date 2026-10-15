#include <llvm/IR/Function.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Verifier.h>
#include "ast.h"

using namespace llvm;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;

std::map<std::string, llvm::Value*> NamedValues;

void InitializeCodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit", *TheContext);
    Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

Value *LogErrorV(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

Value* NumberExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(val));
}

Value* VariableExprAST::codegen() {
    Value *val = NamedValues[name];
    if (!val)
        LogErrorV("Unknown variable name");
    return val;
}

Value* BinaryExprAST::codegen() {
    Value *l = lhs->codegen();
    Value *r = rhs->codegen();
    if (!l || !r)
        return nullptr;

    switch (op) {
        case '+':
            return Builder->CreateFAdd(l, r, "addtmp");
        case '-':
            return Builder->CreateFSub(l, r, "subtmp");
        case '*':
            return Builder->CreateFMul(l, r, "multmp");
        case '<':
            l = Builder->CreateFCmpULT(l, r, "cmptmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder->CreateUIToFP(l, Type::getDoubleTy(*TheContext),
                    "booltmp");
        default:
            return LogErrorV("invalid binary operator");
    }
}

Value* CallExprAST::codegen() {
    // Look up the name in the global module table.
    Function *calleeF = TheModule->getFunction(callee);
    if (!calleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (calleeF->arg_size() != args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> argsV;
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
        argsV.push_back(args[i]->codegen());
        if (!argsV.back())
            return nullptr;
    }

    return Builder->CreateCall(calleeF, argsV, "calltmp");
}

Function *PrototypeAST::codegen() {
    std::vector<Type*> doubles(args.size(), Type::getDoubleTy(*TheContext));

    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), doubles, false);

    Function *F = Function::Create(FT, Function::ExternalLinkage, name, TheModule.get());

    unsigned idx = 0;
    for (auto &arg : F->args())
        arg.setName(args[idx++]);

    return F;
}

Function *FunctionAST::codegen() {
    // First, check for an existing function from a previous 'extern' declaration.
    Function *function = TheModule->getFunction(proto->getName());

    if (!function)
        function = proto->codegen();

    if (!function)
        return nullptr;

    if (!function->empty())
        return (Function*)LogErrorV("Function cannot be redefined.");

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", function);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();
    for (auto &arg : function->args())
        NamedValues[std::string(arg.getName())] = &arg;

    if (Value *retVal = body->codegen()) {
        // Finish off the function.
        Builder->CreateRet(retVal);

        // Validate the generated code, checking for consistency.
        verifyFunction(*function);

        return function;
    }
    // Error reading body, remove function.
    function->eraseFromParent();
    return nullptr;
}
