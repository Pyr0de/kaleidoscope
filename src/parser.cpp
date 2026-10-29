#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include <cstdio>
#include <llvm-22/llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <memory>

std::map<char, int> binOpPrecedence;

static std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}
static std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

/// numberexpr ::= number
std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(lexer.number());
    getNextToken();
    return result;
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
    getNextToken();
    auto val = ParseExpression();
    if (currToken != ')')
        return LogError("Expected ')'");

    getNextToken();
    return val;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    std::string idName = lexer.identifier();

    getNextToken();

    if (currToken != '(')
        return std::make_unique<VariableExprAST>(idName);

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> args;
    if (currToken != ')') {
        while (true) {
            if (auto arg = ParseExpression())
                args.push_back(std::move(arg));
            else
                return nullptr;

            if (currToken == ')')
                break;

            if (currToken != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    getNextToken();

    return std::make_unique<CallExprAST>(idName, std::move(args));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
std::unique_ptr<ExprAST> Parser::ParsePrimary() {
    switch (currToken) {
        case tok_ident:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case tok_if:
            return ParseIfExpr();
        case tok_for:
            return ParseForExpr();
        default:
            return LogError("unknown token when expecting an expression");
    }
}

Parser::Parser() {
    binOpPrecedence['='] = 2;
    binOpPrecedence['<'] = 10;
    binOpPrecedence['+'] = 20;
    binOpPrecedence['-'] = 20;
    binOpPrecedence['*'] = 40;
}

int Parser::GetTokPrecedence() {
    if (!isascii(currToken))
        return -1;

    int precedence = binOpPrecedence[currToken];
    if (precedence <= 0) return -1;

    return precedence;
}

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> Parser::ParseExpression() {
  auto lhs = ParseUnary();
  if (!lhs)
    return nullptr;

  return ParseBinOpRHS(0, std::move(lhs));
}

/// unary
///   ::= primary
///   ::= '!' unary
std::unique_ptr<ExprAST> Parser::ParseUnary() {
    // If the current token is not an operator, it must be a primary expr.
    if (!isascii(currToken) || currToken == '(' || currToken == ',')
        return ParsePrimary();

    // If this is a unary operator, read it.
    int Opc = currToken;
    getNextToken();
    if (auto Operand = ParseUnary())
        return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    return nullptr;
}

/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(
        int ExprPrec,
        std::unique_ptr<ExprAST> lhs
        ) {
    while (true) {
        int precedence = GetTokPrecedence();

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (precedence < ExprPrec)
            return lhs;

        // Okay, we know this is a binop.
        int binOp = currToken;
        getNextToken(); // eat binop

        // Parse the primary expression after the binary operator.
        auto rhs = ParseUnary();
        if (!rhs)
            return nullptr;

        // If BinOp binds less tightly with rhs than the operator after RHS, let
        // the pending operator take rhs as its lhs.
        int nextPrec = GetTokPrecedence();
        if (precedence < nextPrec) {
            rhs = ParseBinOpRHS(precedence + 1, std::move(rhs));
            if (!rhs)
                return nullptr;
        }

        // Merge lhs/rhs.
        lhs = std::make_unique<BinaryExprAST>(binOp, std::move(lhs), std::move(rhs));
    }
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
std::unique_ptr<ExprAST> Parser::ParseIfExpr() {
    getNextToken();  // eat the if.

    // condition.
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (currToken != tok_then)
        return LogError("expected then");
    getNextToken();  // eat the then

    auto Then = ParseExpression();
    if (!Then)
        return nullptr;

    if (currToken != tok_else)
        return LogError("expected else");

    getNextToken();

    auto Else = ParseExpression();
    if (!Else)
        return nullptr;

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
            std::move(Else));
}

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
std::unique_ptr<ExprAST> Parser::ParseForExpr() {
    getNextToken();  // eat the for.

    if (currToken != tok_ident)
        return LogError("expected identifier after for");

    std::string IdName = lexer.identifier();
    getNextToken();  // eat identifier.

    if (currToken != '=')
        return LogError("expected '=' after for");
    getNextToken();  // eat '='.


    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (currToken != ',')
        return LogError("expected ',' after for start value");
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    // The step value is optional.
    std::unique_ptr<ExprAST> Step;
    if (currToken == ',') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }

    if (currToken != tok_in)
        return LogError("expected 'in' after for");
    getNextToken();  // eat 'in'.

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;

    return std::make_unique<ForExprAST>(IdName, std::move(Start),
            std::move(End), std::move(Step),
            std::move(Body));
}

/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> Parser::ParsePrototype() {
    std::string fnName;

    /// 0 = Ident, 1 = Binary, 2 = Unary
    unsigned kind = 0;
    unsigned precedence = 30;

    switch (currToken) {
        default:
            return LogErrorP("Expected function name in prototype");
        case tok_ident:
            fnName = lexer.identifier();
            kind = 0;
            getNextToken();
            break;
        case tok_binary:
            getNextToken();
            if (!isascii(currToken))
                return LogErrorP("Expected binary operator");
            fnName = "binary";
            fnName += (char)currToken;
            kind = 1;

            getNextToken();

            if (currToken == tok_number) {
                int numVal = lexer.number();
                if (numVal < 1 || numVal > 100)
                    return LogErrorP("Invalid precedence: must be 1..100");
                precedence = (unsigned)numVal;
            }
            getNextToken();
            break;
        case tok_unary:
            getNextToken();
            if (!isascii(currToken))
                return LogErrorP("Expected unary operator");
            fnName = "unary";
            fnName += (char)currToken;
            kind = 2;
            getNextToken();
            break;

    }

    if (currToken != '(')
        return LogErrorP("Expected '(' in prototype");

    // Read the list of argument names.
    std::vector<std::string> argNames;
    while (getNextToken() == tok_ident)
        argNames.push_back(lexer.identifier());
    if (currToken != ')')
        return LogErrorP("Expected ')' in prototype");

    getNextToken();


    return std::make_unique<PrototypeAST>(fnName, std::move(argNames), kind != 0, precedence);
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> Parser::ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> Parser::ParseExtern() {
    getNextToken();
    return ParsePrototype();
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> Parser::ParseTopLevelExpr() {
    if (auto expr = ParseExpression()) {
        auto proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(proto), std::move(expr));
    }
    return nullptr;
}

void Parser::HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition:\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            ExitOnErr(JIT->addModule(orc::ThreadSafeModule(
                            std::move(TheModule),
                            std::move(TheContext)
                            )));
            InitializeModuleAndManagers();
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void Parser::HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Read extern:\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void Parser::HandleTopLevelExpression() {
    if (auto FnAST = ParseTopLevelExpr()) {
        if (auto *FnIR = FnAST->codegen()) {
            // FnIR->print(errs());
            auto RT = JIT->getMainJITDylib().createResourceTracker();

            auto TSM = orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(JIT->addModule(std::move(TSM), RT));
            InitializeModuleAndManagers();

            auto ExprSymbol = ExitOnErr(JIT->lookup("__anon_expr"));

            double (*FP)() = ExprSymbol.toPtr<double (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());

            // fprintf(stderr, "Read top-level expression:\n");
            // FnIR->print(errs());
            // fprintf(stderr, "\n");

            // // Remove the anonymous expression.
            // FnIR->eraseFromParent();

            ExitOnErr(RT->remove());
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}
