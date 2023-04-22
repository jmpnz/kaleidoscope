#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstdio>

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

// Parse a number literal.
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();
    return std::move(result);
}

int main(void) {
    std::cout << "Hello fron the LLVM tutorial\n";
    // How do we build a binary expression ?
    auto lhs = std::make_unique<VariableExprAST>("x");
    auto rhs = std::make_unique<VariableExprAST>("y");
    auto result = std::make_unique<BinaryExprAST>('+', std::move(lhs),
            std::move(rhs));
    fprintf(stderr, "ready > ");
}
