#pragma once

#include "CDG/Match.hpp"
#include "DDG/Match.hpp"
#include "Optimization/Optimization.hpp"
#include "Rewriter/Rewrite.hpp"
#include "Types.hpp"
#include "mlir/IR/Block.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/Parser/Parser.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <set>
#include <string>
#include <vector>

struct PatternBranch {
  std::string Kind;
  std::string Condition;
  std::string ReplacementSource;
  bool IsElse = false;
  std::vector<optimization::OptimizationSpec> Optimizations;

  template <class Archive>
  void serialize(Archive &Ar, const unsigned int /*version*/) {
    Ar & Kind;
    Ar & Condition;
    Ar & ReplacementSource;
    Ar & IsElse;
    Ar & Optimizations;
  }
};

class Data {
private:
  friend boost::serialization::access;

  template <class Archive>
  void save(Archive &Ar, const unsigned int /*unused*/) const {
    std::vector<std::string> PatternCodes;
    PatternCodes.reserve(Patterns.size());

    for (const auto &Pat : Patterns)
      PatternCodes.push_back(getModuleCodeAsString(Pat));

    Ar & PatternCodes;
    Ar & PatternBranches;
    Ar & ReplacementLangs;
    Ar & GlobalOptimizations;
  }

  template <class Archive>
  void load(Archive &Ar, const unsigned int /*unused*/) {
    std::vector<std::string> PatternCodes;

    Ar & PatternCodes;
    Ar & PatternBranches;
    Ar & ReplacementLangs;
    Ar & GlobalOptimizations;

    auto Func = [this](const std::string &Code) {
      return mlir::parseSourceString<mlir::ModuleOp>(Code, &Context);
    };
    std::transform(PatternCodes.begin(), PatternCodes.end(),
                   std::back_inserter(Patterns), Func);

    CompiledReplacements.resize(PatternBranches.size());
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()

  using OwningModuleRef = mlir::OwningOpRef<mlir::ModuleOp>;

  static std::string getModuleCodeAsString(const OwningModuleRef &ModuleOp) {
    std::string ModuleCode;
    llvm::raw_string_ostream Ostream(ModuleCode);
    ModuleOp.get().print(Ostream);
    Ostream.flush();
    return ModuleCode;
  }

  mlir::MLIRContext Context;
  std::vector<OwningModuleRef> Inputs;
  std::vector<std::string> InputsFilepaths;

  std::vector<OwningModuleRef> Patterns;
  
  /// Branchs for each pattern [PatternIdx][BranchIdx] 
  std::vector<std::vector<PatternBranch>> PatternBranches;

  /// Cache of compiled replacement modules for each pattern and branch [PatternIdx][BranchIdx] 
  std::vector<std::vector<OwningModuleRef>> CompiledReplacements;

  std::vector<std::string> ReplacementLangs;

  /// Global optimizations that apply to the entire code
  std::vector<optimization::OptimizationSpec> GlobalOptimizations;

  std::vector<cdg::Match> CdgMatches;
  std::vector<ddg::Match> DdgMatches;
  std::vector<Rewrite> Rewrites;

public:
  mlir::MLIRContext *getContext() { return &Context; }

  void setGlobalOptimizations(std::vector<optimization::OptimizationSpec> Opts) {
    GlobalOptimizations = std::move(Opts);
  }

  const std::vector<optimization::OptimizationSpec> &getGlobalOptimizations() const {
    return GlobalOptimizations;
  }

  [[nodiscard]] unsigned getNumPatterns() const { return Patterns.size(); }
  [[nodiscard]] std::string getInputFilepath(int Idx) const { return InputsFilepaths[Idx]; }

  mlir::ModuleOp getInput(int Idx) { return Inputs[Idx].get(); }

  mlir::ModuleOp getPattern(int Idx) { return Patterns[Idx].get(); }

  /// Compiles and returns the replacement module for the specified pattern and branch.
  mlir::ModuleOp getReplacement(int PatternIdx, int BranchIdx);

  const std::vector<PatternBranch> &getBranches(int PatternIdx) const {
    return PatternBranches[PatternIdx];
  }

  std::vector<mlir::ModuleOp> getInputs();
  std::map<int, mlir::Operation *> getPatternRoots();

  void setCdgMatches(std::vector<cdg::Match> &&CdgMatches) {
    this->CdgMatches = std::move(CdgMatches);
  }

  void setDdgMatches(std::vector<ddg::Match> &&DdgMatches) {
    this->DdgMatches = std::move(DdgMatches);
  }

  unsigned addInput(OwningModuleRef &&Module, std::string &&Filepath);
  unsigned addRewrite(OwningModuleRef &&Pattern,
                      std::vector<PatternBranch> &&Branches,
                      std::string &&Lang);
  unsigned addPattern(OwningModuleRef &&Module);

  void reserveRewrites(unsigned int Size) {
    this->Patterns.reserve(Size);
    this->PatternBranches.reserve(Size);
    this->ReplacementLangs.reserve(Size);
    this->CompiledReplacements.reserve(Size);
  }

  mlir::Operation *getPatternRoot(int Idx);
  mlir::Block *getPatternEntryBlock(int Idx);
  std::vector<mlir::Operation *> getCdgCandidates();
  std::vector<Rewrite> &getRewrites();

  [[nodiscard]] std::string
  getOutputFilepath(int Idx, const llvm::StringRef &Suffix) const;

  void dumpCdgMatches();
  void dumpDdgMatches();
  void dumpInputsCode();
  void dumpRewritesCode();
};