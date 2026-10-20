#include "Passes.hpp"
#include "mlir/IR/BuiltinOps.h"

namespace optimization {
namespace {

struct InlinePass
    : public mlir::PassWrapper<InlinePass,
                               mlir::OperationPass<mlir::ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(InlinePass)

  llvm::StringRef getArgument() const final { return "smr-inline"; }
  llvm::StringRef getDescription() const final {
    return "Inline calls to functions defined in the module";
  }

  void runOnOperation() override {}
};

} // namespace

std::unique_ptr<mlir::Pass> createInlinePass() {
  return std::make_unique<InlinePass>();
}

void registerInlinePass() { mlir::PassRegistration<InlinePass>(); }

} // namespace optimization
