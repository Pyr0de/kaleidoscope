#pragma once

#include "lexer.h"
#include "ast.h"
#include <map>
#include <llvm/Support/Error.h>

static ExitOnError ExitOnErr;
extern std::map<char, int> binOpPrecedence;

class Parser {
    Lexer lexer;

    public:
    Parser();

    int currToken;
    int getNextToken() {
        return currToken = lexer.getToken();
    }

    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseParenExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::unique_ptr<ExprAST> ParsePrimary();

    int GetTokPrecedence();

    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int, std::unique_ptr<ExprAST>);

    std::unique_ptr<ExprAST> ParseIfExpr();
    std::unique_ptr<ExprAST> ParseForExpr();

    std::unique_ptr<PrototypeAST> ParsePrototype();
    std::unique_ptr<FunctionAST> ParseDefinition();

    std::unique_ptr<PrototypeAST> ParseExtern();
    std::unique_ptr<FunctionAST> ParseTopLevelExpr();

    void HandleDefinition();
    void HandleExtern();
    void HandleTopLevelExpression();

};
