#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/IR/Function.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <memory>
#include "ast.h"

using namespace llvm;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;

std::map<std::string, llvm::Value*> NamedValues;

static std::unique_ptr<FunctionPassManager> FPM;
static std::unique_ptr<LoopAnalysisManager> LAM;
static std::unique_ptr<FunctionAnalysisManager> FAM;
static std::unique_ptr<CGSCCAnalysisManager> CGAM;
static std::unique_ptr<ModuleAnalysisManager> MAM;
static std::unique_ptr<PassInstrumentationCallbacks> PIC;
static std::unique_ptr<StandardInstrumentations> SI;

void InitializeCodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit", *TheContext);

    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    FPM = std::make_unique<FunctionPassManager>();
    LAM = std::make_unique<LoopAnalysisManager>();
    FAM = std::make_unique<FunctionAnalysisManager>();
    CGAM = std::make_unique<CGSCCAnalysisManager>();
    MAM = std::make_unique<ModuleAnalysisManager>();
    PIC = std::make_unique<PassInstrumentationCallbacks>();
    SI = std::make_unique<StandardInstrumentations>(*TheContext, true);
    SI->registerCallbacks(*PIC, MAM.get());

    // Add transform passes.
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    FPM->addPass(InstCombinePass());
    // Reassociate expressions.
    FPM->addPass(ReassociatePass());
    // Eliminate Common SubExpressions.
    FPM->addPass(GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc)->
    FPM->addPass(SimplifyCFGPass());

    PassBuilder PB;
    PB.registerModuleAnalyses(*MAM);
    PB.registerFunctionAnalyses(*FAM);
    PB.crossRegisterProxies(*LAM, *FAM, *CGAM, *MAM);
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

        FPM->run(*function, *FAM);

        return function;
    }
    // Error reading body, remove function.
    function->eraseFromParent();
    return nullptr;
}
