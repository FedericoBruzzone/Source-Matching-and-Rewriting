#include "Passes.hpp"
#include "clang/CIR/Dialect/IR/CIRDialect.h"

namespace optimization {
namespace {

struct TilingPass
    : public mlir::PassWrapper<TilingPass, mlir::OperationPass<cir::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(TilingPass)

  TilingPass() = default;
  TilingPass(const TilingPass &Other) : PassWrapper(Other) {}
  explicit TilingPass(unsigned SizeValue) { Size = SizeValue; }

  llvm::StringRef getArgument() const final { return "smr-tiling"; }
  llvm::StringRef getDescription() const final {
    return "Tile the cir.for loops";
  }

  void runOnOperation() override {}

  Option<unsigned> Size{*this, "size",
                        llvm::cl::desc("Iterations in each tile"),
                        llvm::cl::init(32)};
};

} // namespace

std::unique_ptr<mlir::Pass> createTilingPass(unsigned Size) {
  return std::make_unique<TilingPass>(Size);
}

void registerTilingPass() { mlir::PassRegistration<TilingPass>(); }

} // namespace optimization
