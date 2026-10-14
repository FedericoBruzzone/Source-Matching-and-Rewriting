//===- Parser.h - Pat Language Parser -------------------------------------===//
//
// This file implements the parser for the Pat language. It processes the Token
// Provided by the Lexer and returns an AST.
//
//===----------------------------------------------------------------------===//

#include "Parser.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AST.hpp"
#include "Lexer.hpp"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

namespace pat {

std::unique_ptr<RootAST> Parser::parse() {
  auto Rewrites = std::make_unique<std::vector<std::unique_ptr<RewriteAST>>>();

  Lexer.getNextToken();

  while (Lexer.getCurToken() != tok_eof) {
    if (Lexer.getCurToken() == '$') {
      if (!parseOptVarDefinition())
        return nullptr;
      continue;
    }

    std::unique_ptr<RewriteAST> Rewrite;
    if (!(Rewrite = parseRewrite()))
      return nullptr;

    Rewrites->push_back(std::move(Rewrite));
  }

  return std::make_unique<RootAST>(std::move(Rewrites), std::move(GlobalOpts));
}

bool Parser::parseOptVarDefinition() {
  Lexer.consume(static_cast<Token>('$'));

  std::string VarName;
  Token Tok = Lexer.getCurToken();
  while (isalnum(static_cast<char>(Tok)) || Tok == '_') {
    VarName += static_cast<char>(Tok);
    Tok = Lexer.getNextToken();
  }

  if (VarName.empty()) {
    parseError<RewriteAST>("identifier", "after '$' in optimization variable definition");
    return false;
  }

  if (Lexer.getCurToken() != tok_equal) {
    parseError<RewriteAST>("=", "after variable name in optimization definition");
    return false;
  }
  Lexer.consume(tok_equal);

  auto OptSpecsOpt = parseOptimizations();
  if (!OptSpecsOpt)
    return false;

  auto OptSpecs = std::move(*OptSpecsOpt);

  if (VarName == "GLOBAL") {
    GlobalOpts.insert(GlobalOpts.end(), OptSpecs.begin(), OptSpecs.end());
  } else {
    OptVars[VarName] = std::move(OptSpecs);
  }

  return true;
}

void Parser::parseOptimizationError(llvm::StringRef OptName) {
  llvm::errs() << "Parse error (" << Lexer.getLastLocation().Line << ", "
               << Lexer.getLastLocation().Col
               << "): unknown or invalid optimization directive '$"
               << OptName << "'\n";
}

std::optional<std::vector<optimization::OptimizationSpec>> Parser::parseOptimizations() {
  std::vector<optimization::OptimizationSpec> Opts;
  if (Lexer.getCurToken() != '[')
    return Opts;

  Lexer.consume(static_cast<Token>('['));

  while (Lexer.getCurToken() != ']' && Lexer.getCurToken() != tok_eof) {
    if (Lexer.getCurToken() == '$') {
      Lexer.consume(static_cast<Token>('$'));
    }

    std::string Name;
    Token Tok = Lexer.getCurToken();
    while (isalnum(static_cast<char>(Tok)) || Tok == '_') {
      Name += static_cast<char>(Tok);
      Tok = Lexer.getNextToken();
    }

    if (Name.empty())
      break;

    if (OptVars.count(Name)) {
      const auto &VarSpecs = OptVars[Name];
      Opts.insert(Opts.end(), VarSpecs.begin(), VarSpecs.end());
    } else {
      optimization::Opt Code = optimization::Optimization::parseKind(Name);
      if (!optimization::Optimization::isValid(Code)) {
        parseOptimizationError(Name);
        return std::nullopt;
      }

      optimization::OptimizationSpec Spec;
      Spec.Code = Code;

      if (Lexer.getCurToken() == '(') {
        Lexer.consume(static_cast<Token>('('));
        std::string CurrentArg;
        int ParenLevel = 1;

        while (ParenLevel > 0 && Lexer.getCurToken() != tok_eof) {
          Tok = Lexer.getCurToken();
          if (Tok == '(') ParenLevel++;
          else if (Tok == ')') {
            ParenLevel--;
            if (ParenLevel == 0) break;
          }

          if (Tok == ',' && ParenLevel == 1) {
            Spec.Args.push_back(llvm::StringRef(CurrentArg).trim().str());
            CurrentArg.clear();
          } else {
            CurrentArg += static_cast<char>(Tok);
          }
          Tok = Lexer.getNextToken(true);
        }

        if (!CurrentArg.empty()) {
          Spec.Args.push_back(llvm::StringRef(CurrentArg).trim().str());
        }

        if (Lexer.getCurToken() == ')') {
          Lexer.consume(static_cast<Token>(')'));
        }
      }

      Opts.push_back(Spec);
    }

    if (Lexer.getCurToken() == ',') {
      Lexer.consume(static_cast<Token>(','));
    }
  }

  if (Lexer.getCurToken() == ']') {
    Lexer.consume(static_cast<Token>(']'));
  }

  return Opts;
}

std::unique_ptr<LangAST> Parser::parseLang() {
  std::string Lang;
  auto LastChar = Lexer.getCurToken();

  if (isalpha((char)LastChar) != 0) {
    Lang += (char)LastChar;
    while (isalnum(LastChar = Lexer.getNextToken()) != 0)
      Lang += (char)LastChar;
  }

  return std::make_unique<LangAST>(std::move(Lang));
}

std::unique_ptr<BlockAST> Parser::parseBlock() {
  std::string Block;
  std::string Prev;
  Token Token = Lexer.getCurToken();
  int Level = 1;

  if (Token != tok_brace_open)
    return parseError<BlockAST>("{", "to begin block");

  while (Level != 0 && Token != tok_eof) {
    Block += Prev;
    Token = Lexer.getNextToken(true);
    Prev = Token;

    if (Token == tok_brace_open) {
      ++Level;
    } else if (Token == tok_brace_close) {
      --Level;
    }
  }

  if (Token != tok_brace_close)
    return parseError<BlockAST>("}", "to close block");

  Lexer.consume(Token);
  return std::make_unique<BlockAST>(std::move(Block));
}

/// Parses condition inside ( condition_expression ).
std::unique_ptr<ConditionAST> Parser::parseCondition() {
  if (Lexer.getCurToken() != '(')
    return parseError<ConditionAST>("(", "after directive condition");

  std::string CondStr;
  std::string Prev;
  Token Tok = Lexer.getCurToken();
  int Level = 1;

  while (Level != 0 && Tok != tok_eof) {
    CondStr += Prev;
    Tok = Lexer.getNextToken(true);
    Prev = Tok;

    if (Tok == '(') {
      ++Level;
    } else if (Tok == ')') {
      --Level;
    }
  }

  if (Tok != ')')
    return parseError<ConditionAST>(")", "to close condition expression");

  Lexer.consume(Tok);
  return std::make_unique<ConditionAST>(std::move(CondStr));
}

/// Parses a rewrite: lang { pattern } [$if (...) = { replacement }] [$elif ...] [$else ...]
std::unique_ptr<RewriteAST> Parser::parseRewrite() {
  auto Lang = parseLang();
  std::unique_ptr<pat::BlockAST> Pattern;

  if (!(Pattern = parseBlock()))
    return nullptr;

  std::vector<std::unique_ptr<RewriteBranchAST>> Branches;

  auto getValidBlock = [&](std::unique_ptr<pat::BlockAST> Block) -> std::unique_ptr<pat::BlockAST> {
    if (!Block) return nullptr;
    if (Block->str().empty()) {
      return std::make_unique<pat::BlockAST>(Pattern->str());
    }
    return Block;
  };

  if (Lexer.getCurToken() == tok_equal) {
    Lexer.consume(tok_equal);
    auto Replacement = getValidBlock(parseBlock());
    if (!Replacement) return nullptr;

    std::vector<optimization::OptimizationSpec> Opts;
    if (Lexer.getCurToken() == '[') {
      auto OptSpecsOpt = parseOptimizations();
      if (!OptSpecsOpt) return nullptr;
      Opts = std::move(*OptSpecsOpt);
    }

    Branches.push_back(std::make_unique<RewriteBranchAST>(
        "", nullptr, std::move(Replacement), std::move(Opts)));
    return std::make_unique<RewriteAST>(std::move(Lang), std::move(Pattern),
                                        std::move(Branches));
  }

  while (Lexer.getCurToken() == '$') {
    std::string Tag = "$";
    Token Tok = Lexer.getNextToken();
    while (isalpha(static_cast<char>(Tok)) != 0) {
      Tag += static_cast<char>(Tok);
      Tok = Lexer.getNextToken();
    }

    if (Tag == "$if" || Tag == "$elif") {
      auto Cond = parseCondition();
      if (!Cond) return nullptr;

      if (Lexer.getCurToken() != tok_equal)
        return parseError<RewriteAST>("=", "after condition expression");
      Lexer.consume(tok_equal);

      auto Repl = getValidBlock(parseBlock());
      if (!Repl) return nullptr;

      std::vector<optimization::OptimizationSpec> Opts;
      if (Lexer.getCurToken() == '[') {
        auto OptSpecsOpt = parseOptimizations();
        if (!OptSpecsOpt) return nullptr;
        Opts = std::move(*OptSpecsOpt);
      }

      Branches.push_back(std::make_unique<RewriteBranchAST>(
          Tag, std::move(Cond), std::move(Repl), std::move(Opts)));
    } else if (Tag == "$else") {
      if (Lexer.getCurToken() != tok_equal)
        return parseError<RewriteAST>("=", "after $else directive");
      Lexer.consume(tok_equal);

      auto Repl = getValidBlock(parseBlock());
      if (!Repl) return nullptr;

      std::vector<optimization::OptimizationSpec> Opts;
      if (Lexer.getCurToken() == '[') {
        auto OptSpecsOpt = parseOptimizations();
        if (!OptSpecsOpt) return nullptr;
        Opts = std::move(*OptSpecsOpt);
      }

      Branches.push_back(std::make_unique<RewriteBranchAST>(
          Tag, nullptr, std::move(Repl), std::move(Opts)));
    } else {
      return parseError<RewriteAST>("$if, $elif, or $else", "for conditional branch");
    }
  }

  if (Branches.empty()) {
    auto DefaultRepl = std::make_unique<pat::BlockAST>(Pattern->str());
    std::vector<optimization::OptimizationSpec> Opts;
    if (Lexer.getCurToken() == '[') {
      auto OptSpecsOpt = parseOptimizations();
      if (!OptSpecsOpt) return nullptr;
      Opts = std::move(*OptSpecsOpt);
    }

    Branches.push_back(std::make_unique<RewriteBranchAST>(
        "", nullptr, std::move(DefaultRepl), std::move(Opts)));
  }

  return std::make_unique<RewriteAST>(std::move(Lang), std::move(Pattern),
                                      std::move(Branches));
}

template <typename R, typename T, typename U>
std::unique_ptr<R> Parser::parseError(T &&Expected, U &&Context) {
  auto CurToken = Lexer.getCurToken();

  llvm::errs() << "Parse error (" << Lexer.getLastLocation().Line << ", "
               << Lexer.getLastLocation().Col << "): expected '"
               << static_cast<const char *>(Expected) << "' "
               << static_cast<const char *>(Context) << " but has Token "
               << CurToken;

  if (isprint(CurToken))
    llvm::errs() << " '" << (char)CurToken << "'";

  llvm::errs() << "\n";
  return nullptr;
}

} // namespace pat