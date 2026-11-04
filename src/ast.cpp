#include <cstdio>
#include <llvm/TargetParser/Triple.h>
#include <llvm/IR/Instructions.h>
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
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include <memory>
#include <string>
#include <vector>
#include "ast.h"
#include "parser.h"
#include "debug.h"

using namespace llvm;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;

std::map<std::string, llvm::AllocaInst*> NamedValues;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

// static std::unique_ptr<FunctionPassManager> FPM;
// static std::unique_ptr<LoopAnalysisManager> LAM;
// static std::unique_ptr<FunctionAnalysisManager> FAM;
// static std::unique_ptr<CGSCCAnalysisManager> CGAM;
// static std::unique_ptr<ModuleAnalysisManager> MAM;
// static std::unique_ptr<PassInstrumentationCallbacks> PIC;
// static std::unique_ptr<StandardInstrumentations> SI;

// std::unique_ptr<orc::KaleidoscopeJIT> JIT;

void InitializeModuleAndManagers(TargetMachine* targetMachine, llvm::Triple triple) {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("my cool jit", *TheContext);
    TheModule->setDataLayout(targetMachine->getTargetTriple().computeDataLayout());
    TheModule->setTargetTriple(triple);

    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    InitializeDebug();

    // FPM = std::make_unique<FunctionPassManager>();
    // LAM = std::make_unique<LoopAnalysisManager>();
    // FAM = std::make_unique<FunctionAnalysisManager>();
    // CGAM = std::make_unique<CGSCCAnalysisManager>();
    // MAM = std::make_unique<ModuleAnalysisManager>();
    // PIC = std::make_unique<PassInstrumentationCallbacks>();
    // SI = std::make_unique<StandardInstrumentations>(*TheContext, true);
    // SI->registerCallbacks(*PIC, MAM.get());

    // // Add transform passes.
    // // Do simple "peephole" optimizations and bit-twiddling optzns.
    // FPM->addPass(InstCombinePass());
    // // Reassociate expressions.
    // FPM->addPass(ReassociatePass());
    // // Eliminate Common SubExpressions.
    // FPM->addPass(GVNPass());
    // // Simplify the control flow graph (deleting unreachable blocks, etc)->
    // FPM->addPass(SimplifyCFGPass());
    // // Promote allocas to registers.
    // FPM->addPass(PromotePass());
    // // Do simple "peephole" optimizations and bit-twiddling optzns.
    // FPM->addPass(InstCombinePass());
    // // Reassociate expressions.
    // FPM->addPass(ReassociatePass());

    // PassBuilder PB;
    // PB.registerModuleAnalyses(*MAM);
    // PB.registerFunctionAnalyses(*FAM);
    // PB.crossRegisterProxies(*LAM, *FAM, *CGAM, *MAM);
}

Value *LogErrorV(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

Value* NumberExprAST::codegen() {
    KSDbgInfo.emitLocation(this);
    return ConstantFP::get(*TheContext, APFloat(val));
}

static AllocaInst* CreateEntryBlockAlloca(
        Function *TheFunction,
        StringRef VarName
        ) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());

    return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, VarName);
}

Value* VariableExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    AllocaInst *A = NamedValues[name];
    if (!A)
        LogErrorV("Unknown variable name");

    return Builder->CreateLoad(A->getAllocatedType(), A, name.c_str());
}

Value* UnaryExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    Value *OperandV = operand->codegen();
    if (!OperandV)
        return nullptr;

    Function *F = getFunction(std::string("unary") + op);
    if (!F)
        return LogErrorV("Unknown unary operator");

    return Builder->CreateCall(F, OperandV, "unop");
}

Value* BinaryExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    if (op == '=') {
        VariableExprAST* lhsExpr = static_cast<VariableExprAST*>(lhs.get());
        if (!lhsExpr)
            return LogErrorV("destination of '=' must be a variable");

        Value *Val = rhs->codegen();
        if (!Val)
            return nullptr;

        AllocaInst *Alloca = NamedValues[lhsExpr->name];
        if (!Alloca)
            return LogErrorV("Unknown variable name");

        Builder->CreateStore(Val, Alloca);
        return Val;
    }

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
            break;
    }

    Function* func = getFunction(std::string("binary") + op);
    if (!func)
        return LogErrorV("invalid binary operator");

    Value* ops[2] = {l, r};
    return Builder->CreateCall(func, ops, "binops");
}

Function* getFunction(std::string name) {
    if (auto* func = TheModule->getFunction(name))
        return func;

    auto fi = FunctionProtos.find(name);
    if (fi != FunctionProtos.end())
        return fi->second->codegen();

    return nullptr;
}

Value* CallExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    // Look up the name in the global module table.
    Function *calleeF = getFunction(callee);
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
    auto &P = *proto;
    FunctionProtos[proto->getName()] = std::move(proto);

    Function *function = getFunction(P.getName());
    if (!function)
        return nullptr;

    if (P.isBinaryOp())
        binOpPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", function);
    Builder->SetInsertPoint(BB);

    DIFile *Unit = DBuilder->createFile(KSDbgInfo.TheCU->getFilename(), KSDbgInfo.TheCU->getDirectory());

    DIScope *FContext = Unit;
    unsigned LineNo = 0;
    unsigned ScopeLine = 0;
    DISubprogram *SP = DBuilder->createFunction(
            FContext, P.getName(), StringRef(), Unit, LineNo,
            CreateFunctionType(function->arg_size()),
            ScopeLine,
            DINode::FlagPrototyped,
            DISubprogram::SPFlagDefinition);
    function->setSubprogram(SP);

    KSDbgInfo.LexicalBlocks.push_back(SP);

    KSDbgInfo.emitLocation(nullptr);

    NamedValues.clear();
    unsigned argIdx = 0;
    for (auto &arg : function->args()) {
        AllocaInst* alloca = CreateEntryBlockAlloca(function, arg.getName());

        DILocalVariable *D = DBuilder->createParameterVariable(
                SP,
                arg.getName(),
                ++argIdx,
                Unit,
                LineNo,
                KSDbgInfo.getDoubleTy(),
                true
                );

        DBuilder->insertDeclare(
                alloca,
                D,
                DBuilder->createExpression(),
                DILocation::get(SP->getContext(), LineNo, 0, SP),
                Builder->GetInsertBlock()
                );

        Builder->CreateStore(&arg, alloca);

        NamedValues[std::string(arg.getName())] = alloca;
    }

    KSDbgInfo.emitLocation(body.get());

    if (Value *retVal = body->codegen()) {
        // Finish off the function.
        Builder->CreateRet(retVal);

        // Validate the generated code, checking for consistency.
        verifyFunction(*function);

        // FPM->run(*function, *FAM);
        KSDbgInfo.LexicalBlocks.pop_back();

        return function;
    }
    // Error reading body, remove function.
    function->eraseFromParent();

    if (P.isBinaryOp())
        binOpPrecedence.erase(proto->getOperatorName());

    KSDbgInfo.LexicalBlocks.pop_back();

    return nullptr;
}

Value* IfExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    Value *cond = Cond->codegen();
    if (!cond)
        return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    cond = Builder->CreateFCmpONE(cond, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");
    Function *Func = Builder->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", Func);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(cond, ThenBB, ElseBB);

    // Emit then value.
    Builder->SetInsertPoint(ThenBB);

    Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;

    Builder->CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder->GetInsertBlock();

    // Emit else block.
    Func->insert(Func->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    Value *ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;

    Builder->CreateBr(MergeBB);
    // codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = Builder->GetInsertBlock();

    // Emit merge block.
    Func->insert(Func->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);
    PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

Value* ForExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    Function* function = Builder->GetInsertBlock()->getParent();
    AllocaInst *alloca = CreateEntryBlockAlloca(function, VarName);

    Value* startVal = Start->codegen();

    if (!startVal)
        return nullptr;

    Builder->CreateStore(startVal, alloca);

    BasicBlock* loopBB = BasicBlock::Create(*TheContext, "loop", function);
    BasicBlock* bodyBB = BasicBlock::Create(*TheContext, "loopbody", function);
    BasicBlock *afterBB = BasicBlock::Create(*TheContext, "afterloop", function);

    Builder->CreateBr(loopBB);

    Builder->SetInsertPoint(loopBB);

    AllocaInst *oldVal = NamedValues[VarName];
    NamedValues[VarName] = alloca;

    Value* endVal = End->codegen();
    if (!endVal)
        return nullptr;

    endVal = Builder->CreateFCmpONE(endVal, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

    Builder->CreateCondBr(endVal, bodyBB, afterBB);

    Builder->SetInsertPoint(bodyBB);

    if (!Body->codegen())
        return nullptr;

    Value* stepVal = nullptr;
    if (Step) {
        stepVal = Step->codegen();
        if (!stepVal)
            return nullptr;
    } else {
        stepVal = ConstantFP::get(*TheContext, APFloat(1.0));
    }

    Value* currVar = Builder->CreateLoad(alloca->getAllocatedType(), alloca, VarName.c_str());
    Value* nextVar = Builder->CreateFAdd(currVar, stepVal, "nextVar");
    Builder->CreateStore(nextVar, alloca);

    Builder->CreateBr(loopBB);

    Builder->SetInsertPoint(afterBB);

    if (oldVal)
        NamedValues[VarName] = oldVal;
    else
        NamedValues.erase(VarName);

    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value* VarExprAST::codegen() {
    KSDbgInfo.emitLocation(this);

    std::vector<AllocaInst*> oldBindings;

    Function* function = Builder->GetInsertBlock()->getParent();

    for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
        const std::string &varName = VarNames[i].first;
        ExprAST* init = VarNames[i].second.get();

        Value* initVal;
        if (init) {
            initVal = init->codegen();
            if(!initVal) {
                return nullptr;
            }
        } else {
            initVal = ConstantFP::get(*TheContext, APFloat(0.0));
        }

        AllocaInst *alloca = CreateEntryBlockAlloca(function, varName);
        Builder->CreateStore(initVal, alloca);

        oldBindings.push_back(NamedValues[varName]);

        NamedValues[varName] = alloca;
    }

    Value* bodyVal = Body->codegen();
    if (!bodyVal) {
        return nullptr;
    }

    for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
        NamedValues[VarNames[i].first] = oldBindings[i];

    return bodyVal;
}
