#include "Passes.hpp"
#include "clang/CIR/Dialect/IR/CIRDialect.h"

namespace optimization {
namespace {

struct UnrollPass
    : public mlir::PassWrapper<UnrollPass, mlir::OperationPass<cir::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(UnrollPass)

  UnrollPass() = default;
  UnrollPass(const UnrollPass &Other) : PassWrapper(Other) {}
  explicit UnrollPass(unsigned FactorValue) { Factor = FactorValue; }

  llvm::StringRef getArgument() const final { return "smr-unroll"; }
  llvm::StringRef getDescription() const final {
    return "Unroll the innermost cir.for loops";
  }

  void runOnOperation() override {}

  Option<unsigned> Factor{*this, "factor",
                          llvm::cl::desc("Number of copies of the body"),
                          llvm::cl::init(4)};
};

} // namespace

std::unique_ptr<mlir::Pass> createUnrollPass(unsigned Factor) {
  return std::make_unique<UnrollPass>(Factor);
}

void registerUnrollPass() { mlir::PassRegistration<UnrollPass>(); }

} // namespace optimization
