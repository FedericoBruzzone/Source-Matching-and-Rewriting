#include "Passes.hpp"
#include "mlir/IR/BuiltinOps.h"

namespace optimization {
namespace {

struct DCEPass
    : public mlir::PassWrapper<DCEPass, mlir::OperationPass<mlir::ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(DCEPass)

  llvm::StringRef getArgument() const final { return "smr-dce"; }
  llvm::StringRef getDescription() const final {
    return "Erase dead operations and unused private functions";
  }

  void runOnOperation() override {}
};

} // namespace

std::unique_ptr<mlir::Pass> createDCEPass() {
  return std::make_unique<DCEPass>();
}

void registerDCEPass() { mlir::PassRegistration<DCEPass>(); }

} // namespace optimization
