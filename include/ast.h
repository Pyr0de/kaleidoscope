#pragma once

#include "lexer.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

class PrototypeAST;

extern std::unique_ptr<LLVMContext> TheContext;
extern std::unique_ptr<Module> TheModule;
extern std::unique_ptr<IRBuilder<>> Builder;

extern std::map<std::string, llvm::AllocaInst*> NamedValues;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

// extern std::unique_ptr<orc::KaleidoscopeJIT> JIT;

void InitializeModuleAndManagers(TargetMachine* targetMachine, llvm::Triple triple);

Function* getFunction(std::string name);

class ExprAST {
    SourceLocation loc;
    public:
        ExprAST(SourceLocation loc = CurLoc) : loc(loc) {}
        virtual ~ExprAST() = default;
        virtual Value* codegen() = 0;
        int getLine() const { return loc.Line; }
        int getCol() const { return loc.Col; }
        virtual raw_ostream &dump(raw_ostream &out, int ind) {
            return out << ':' << getLine() << ':' << getCol() << '\n';
        }
};

class NumberExprAST : public ExprAST {
    double val;

    public:
    NumberExprAST(double val) : val(val) {}
    Value* codegen() override;
};

class VariableExprAST : public ExprAST {
    public:
    std::string name;
    VariableExprAST(const std::string &name) : name(name) {}
    Value* codegen() override;
};

class VarExprAST : public ExprAST {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
    std::unique_ptr<ExprAST> Body;

    public:
    VarExprAST(
            std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
            std::unique_ptr<ExprAST> Body
            )
        : VarNames(std::move(VarNames)), Body(std::move(Body)) {}
    Value* codegen() override;
};

class UnaryExprAST : public ExprAST {
    char op;
    std::unique_ptr<ExprAST> operand;

    public:
    UnaryExprAST(char op, std::unique_ptr<ExprAST> operand)
        : op(op), operand(std::move(operand)) {}
    Value* codegen() override;
};

class BinaryExprAST : public ExprAST {
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;

    public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs,
            std::unique_ptr<ExprAST> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    Value* codegen() override;
};

class CallExprAST : public ExprAST {
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;

    public:
    CallExprAST(const std::string &callee,
            std::vector<std::unique_ptr<ExprAST>> args)
        : callee(callee), args(std::move(args)) {}
    Value* codegen() override;
};

class PrototypeAST {
    std::string name;
    std::vector<std::string> args;

    bool isOperator;
    unsigned precedence;

    public:
    PrototypeAST(
            const std::string &name,
            std::vector<std::string> args,
            bool isOperator = 0,
            unsigned precedence = 30
            ) :
        name(name),
        args(std::move(args)),
        isOperator(isOperator),
        precedence(precedence)
    {}

    const std::string &getName() const { return name; }
    Function* codegen();

    bool isUnaryOp() const { return isOperator && args.size() == 1; }
    bool isBinaryOp() const { return isOperator && args.size() == 2; }

    char getOperatorName() const {
        assert(isUnaryOp() || isBinaryOp());
        return name[name.size() - 1];
    }

    unsigned getBinaryPrecedence() const { return precedence; }
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;

    public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto,
            std::unique_ptr<ExprAST> body)
        : proto(std::move(proto)), body(std::move(body)) {}
    Function* codegen();
};

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;

    public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
          std::unique_ptr<ExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    Value* codegen() override;
};

class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

    public:
    ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
            std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
            std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
        Step(std::move(Step)), Body(std::move(Body)) {}

    Value *codegen() override;
};
