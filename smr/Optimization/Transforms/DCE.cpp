#include "Passes.hpp"
#include "mlir/Transforms/Passes.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"

namespace optimization {
namespace {

/// An alloca that is only stored to is never read: the slot, its stores and
/// nothing else can go. A store of a block argument is kept because it fills a
/// parameter slot, which the rewriter maps onto a variable of the matched code
/// and so is read from outside the function.
struct EraseStoreOnlyAllocasPass
    : public mlir::PassWrapper<EraseStoreOnlyAllocasPass,
                               mlir::OperationPass<cir::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(EraseStoreOnlyAllocasPass)

  llvm::StringRef getArgument() const final {
    return "smr-erase-store-only-allocas";
  }
  llvm::StringRef getDescription() const final {
    return "Erase allocas that are only stored to";
  }

  static bool isRemovableStore(mlir::Operation *User, cir::AllocaOp Alloca) {
    auto Store = mlir::dyn_cast<cir::StoreOp>(User);
    return Store && Store.getAddr() == Alloca.getResult() &&
           !Store.getIsVolatile() && !Store.getMemOrder() &&
           !mlir::isa<mlir::BlockArgument>(Store.getValue());
  }

  void runOnOperation() override {
    llvm::SmallVector<cir::AllocaOp> Dead;
    getOperation().walk([&](cir::AllocaOp Alloca) {
      if (llvm::all_of(Alloca->getUsers(), [&](mlir::Operation *User) {
            return isRemovableStore(User, Alloca);
          }))
        Dead.push_back(Alloca);
    });
    for (cir::AllocaOp Alloca : Dead) {
      for (mlir::Operation *User :
           llvm::make_early_inc_range(Alloca->getUsers()))
        User->erase();
      Alloca->erase();
    }
  }
};

} // namespace

void addDCEPasses(mlir::OpPassManager &PM) {
  PM.addNestedPass<cir::FuncOp>(std::make_unique<EraseStoreOnlyAllocasPass>());
  PM.addPass(mlir::createCanonicalizerPass());
  PM.addPass(mlir::createSymbolDCEPass());
}

void registerDCEPipeline() {
  mlir::PassPipelineRegistration<>(
      "smr-dce", "Erase dead operations and unused private functions",
      addDCEPasses);
}

} // namespace optimization
