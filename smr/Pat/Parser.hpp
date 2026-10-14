//===- Parser.h - Pat Language Parser -------------------------------------===//
//
// This file implements the parser for the Pat language. It processes the Token
// provided by the Lexer and returns an AST.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <unordered_map>
#include "AST.hpp"
#include "Lexer.hpp"
#include "Optimization/Optimization.hpp"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

namespace pat {

/// PAT language parser.
class Parser {
public:
  /// Creates a Parser for the supplied lexer.
  explicit Parser(LexerBuffer &Lexer) : Lexer(Lexer) {}

  /// Parse a list of rewrites that constitute the PAT file.
  std::unique_ptr<RootAST> parse();

private:
  Lexer &Lexer;

  /// Tabela de símbolos para variáveis de otimização ($var_name = [...])
  std::unordered_map<std::string, std::vector<optimization::OptimizationSpec>> OptVars;
  std::vector<optimization::OptimizationSpec> GlobalOpts;

  bool parseOptVarDefinition();
  std::unique_ptr<LangAST> parseLang();

  /// Parse a block: a string warpped in curly braces with any characters
  /// allowed.
  ///
  /// block ::= { any_string }
  std::unique_ptr<BlockAST> parseBlock();

  /// Parse a rewrite: pattern block and equivalent replace block.
  ///
  /// rewrite ::= lang { block } = { block } | tok_eof
  std::unique_ptr<RewriteAST> parseRewrite();

  /// Parse a condition: a if and a string wrapped in parentheses.
  ///
  /// rewrite ::= if (any_string)= 
  std::unique_ptr<ConditionAST> parseCondition();
  std::optional<std::vector<optimization::OptimizationSpec>> parseOptimizations();

  template <typename R, typename T, typename U = const char *>
  std::unique_ptr<R> parseError(T &&Expected, U &&Context = "");
  void parseOptimizationError(llvm::StringRef OptName);
};

} // namespace pat