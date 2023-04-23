#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <map>
#include <cstdio>
#include <cctype>
#include <cstdlib>

/**
 * Kaleidoscope is an untyped language with syntax similar to Python
 * and uses the 64-bit floating point type for all values (pattern
 * similar to NaN boxing in Lisps).
 *
 * Example of a function in Kaleidoscope:
 *
 * def fib(x)
 *  if x < 3 then
 *    1
 *  else
 *    fib(x - 1) + fib(x - 2)
 */

// The lexer returns tokens defined below.
enum Token {
  tok_eof = - 1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal; // Filled in if tok_number

// Return the next token from stdin.
static int getTok() {
  static int LastChar = ' ';

  // Skip whitespace
  while(isspace(LastChar))
      LastChar = getchar();
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
        IdentifierStr += LastChar;
    if (IdentifierStr == "def")
        return tok_def;
    if (IdentifierStr == "extern")
        return tok_extern;
    return tok_identifier;
  }
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
        NumStr += LastChar;
        LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }
  if (LastChar == '#') {
      // Comment until end of line.
      do
          LastChar = getchar();
      while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

      if (LastChar != EOF)
          return getTok();
  }
  if (LastChar == EOF)
      return tok_eof;
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

// ExprAST - Base class for all expression nodes.
class ExprAST {
    public:
        virtual ~ExprAST() = default;
};

// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST: public ExprAST {
    double Val;

    public:
    NumberExprAST(double Val) : Val(Val) {}
};

// VariableExprAST - Expression class for referencing a variable.
class VariableExprAST : public ExprAST {
    std::string Name; // Variable name.
    public:
    VariableExprAST(const std::string &Name) : Name(Name) {}
};

// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    char Op; // Binary operator for the expression.
    std::unique_ptr<ExprAST> Lhs, Rhs; // Left and right hand side expressions.
    public:
    BinaryExprAST(char Op, std::unique_ptr<ExprAST> Lhs,
        std::unique_ptr<ExprAST> Rhs)
      : Op(Op), Lhs(std::move(Lhs)), Rhs(std::move(Rhs)) {}
};

// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

    public:
    CallExprAST(const std::string &Callee,
            std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}
};

// PrototypeAST - This class represents the "prototype" for a function,
// which captures its name and argument names.
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;

    public:
    PrototypeAST(const std::string &Name,std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}

    const std::string &getName() const { return Name; }
};

// FunctionAST - This class represents a function definition.
class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

    public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
            std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}
};

static int CurTok;
static int getNextToken() {
    return CurTok = getTok();
}

// Log a parsing error.
std::unique_ptr<ExprAST> LogError(const char* Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char* Str) {
    LogError(Str);
    return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

// Parse a number literal.
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(result);
}

// Parse a parenthesized expression.
static std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if (!V) {
        return nullptr;
    }
    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken();
    return V;
}

// Parse identifier expressions.
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;
    getNextToken();

    if (CurTok != '(') // Not a function call, defo a variable.
        return std::make_unique<VariableExprAST>(IdName);

    // It's a function call
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;
            if (CurTok== ')')
                break;
            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// Parse primary expressions (identifiers, number literals and parenthesized
// expressions).
static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:
            return LogError("unknown token, expecting expression");
          case tok_identifier:
            return ParseIdentifierExpr();
          case tok_number:
            return ParseNumberExpr();
          case '(':
            return ParseParenExpr();
    }
}

// Parsing binary expressions.
static std::map<char, int> BinopPrecedence;

// GetTokPrecedence - Get the precedence of the pending binary operator
// token.
static int GetTokPrecedence() {
    if (isascii(CurTok))
        return -1;

    // Is it a valid binary operation.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

// Parse binary operation right hand side.
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
        std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec)
            return LHS;
        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
                std::move(RHS));
    }
}

// Parse expression implementation.
static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}


// Parse prototype functions.
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype");

    std::string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier)
        ArgNames.push_back(IdentifierStr);
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    getNextToken();

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// Parse function definitions.
static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

// Parse extern expressions.
static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();
    return ParsePrototype();
}

// Parse top level expression.
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

// Main parser loop.
static void MainLoop() {
    while (true) {
        fprintf(stderr, "ready> ");
        switch(CurTok) {
        case tok_eof:
          return;
        case ';':
          getNextToken();
          break;
        case tok_def:
          HandleDefinition();
          break;
        default:
          HandleTopLevelExpression();
          break;
        }
    }
}


int main(void) {
    // Fill the binary precedence map
     BinopPrecedence['<'] = 10;
     BinopPrecedence['+'] = 20;
     BinopPrecedence['-'] = 20;
     BinopPrecedence['*'] = 40;

     fprintf(stderr, "ready> ");
     getNextToken();
     MainLoop();

     return 0;
}
