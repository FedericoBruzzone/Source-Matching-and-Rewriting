#include "Analysis/CanonicalLoop.hpp"
#include "Passes.hpp"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSet.h"

namespace optimization {
namespace {

/// An integer division traps on a zero divisor, so it cannot run in a place
/// where the loop might not have run it.
bool mayTrap(mlir::Operation *Op) {
  auto Bin = mlir::dyn_cast<cir::BinOp>(Op);
  return Bin && mlir::isa<cir::IntType>(Bin.getType()) &&
         (Bin.getKind() == cir::BinOpKind::Div ||
          Bin.getKind() == cir::BinOpKind::Rem);
}

/// Op runs inside the cond or step of a loop, which later passes expect to
/// find as they are.
bool isInLoopHeader(mlir::Operation *Op, cir::ForOp Loop) {
  for (mlir::Region *Region = Op->getParentRegion();
       Region && Region->getParentOp() != Loop.getOperation();
       Region = Region->getParentOp()->getParentRegion()) {
    auto Inner = mlir::dyn_cast<cir::LoopOpInterface>(Region->getParentOp());
    if (Inner && Region != &Inner.getBody())
      return true;
  }
  return false;
}

/// Moves out of Loop, to just before it, the operations of its body that
/// compute the same value in every iteration:
///
///   - operations without effects, except integer division,
///   - loads of slots the loop does not write. A slot whose address is never
///     taken (isPrivateSlot) cannot be written by a call or through a pointer
///     either, so this is where clang's scalars, which all live in memory, are
///     found,
///   - calls to the functions in Pure with arguments that are not pointers.
///     The pattern asserts these have no effect and always return.
///
/// Everything moved is safe to run when the loop does not, and an operation
/// that uses a moved value moves too.
class Hoister {
public:
  Hoister(cir::ForOp Loop, const llvm::StringSet<> &Pure)
      : Loop(Loop), Pure(Pure) {
    Loop->walk([&](cir::StoreOp Store) {
      if (auto Slot = Store.getAddr().getDefiningOp<cir::AllocaOp>())
        Written.insert(Slot);
    });
  }

  unsigned run() {
    llvm::SmallVector<mlir::Operation *> Moved;
    Loop.getBody().walk<mlir::WalkOrder::PreOrder>([&](mlir::Operation *Op) {
      if (canHoist(Op)) {
        Hoisted.insert(Op);
        Moved.push_back(Op);
      }
    });
    for (mlir::Operation *Op : Moved)
      Op->moveBefore(Loop);
    return Moved.size();
  }

private:
  bool isInvariant(mlir::Value Value) {
    if (mlir::Operation *Def = Value.getDefiningOp())
      return Hoisted.contains(Def) || !Loop->isAncestor(Def);
    return !Loop->isAncestor(Value.getParentRegion()->getParentOp());
  }

  bool canHoist(mlir::Operation *Op) {
    if (llvm::any_of(Op->getRegions(),
                     [](mlir::Region &Region) { return !Region.empty(); }) ||
        isInLoopHeader(Op, Loop) ||
        !llvm::all_of(Op->getOperands(),
                      [&](mlir::Value V) { return isInvariant(V); }))
      return false;

    if (auto Load = mlir::dyn_cast<cir::LoadOp>(Op)) {
      auto Slot = Load.getAddr().getDefiningOp<cir::AllocaOp>();
      return Slot && isPrivateSlot(Slot) && !Written.contains(Slot) &&
             !Load.getIsVolatile() && !Load.getMemOrder();
    }
    if (auto Call = mlir::dyn_cast<cir::CallOp>(Op)) {
      return !Call.isIndirect() && Pure.contains(*Call.getCallee()) &&
             Call->getNumResults() <= 1 &&
             !llvm::any_of(Call->getOperandTypes(), [](mlir::Type Type) {
               return mlir::isa<cir::PointerType>(Type);
             });
    }
    return mlir::isPure(Op) && !mayTrap(Op);
  }

  cir::ForOp Loop;
  const llvm::StringSet<> &Pure;
  llvm::SmallPtrSet<mlir::Operation *, 8> Written;
  llvm::SmallPtrSet<mlir::Operation *, 16> Hoisted;
};

struct LICMPass
    : public mlir::PassWrapper<LICMPass, mlir::OperationPass<cir::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LICMPass)

  LICMPass() = default;
  LICMPass(const LICMPass &Other)
      : PassWrapper(Other), Changed(Other.Changed) {}
  LICMPass(llvm::ArrayRef<std::string> PureFunctions, ChangeCounter *Counter)
      : Changed(Counter) {
    Pure = PureFunctions;
  }

  llvm::StringRef getArgument() const final { return "smr-licm"; }
  llvm::StringRef getDescription() const final {
    return "Hoist loop invariant code out of cir.for loops";
  }

  void runOnOperation() override {
    llvm::StringSet<> PureNames;
    for (const std::string &Name : Pure)
      PureNames.insert(Name);

    // Inner loops first, so that what they leave behind can move again.
    unsigned Hoisted = 0;
    getOperation().walk(
        [&](cir::ForOp Loop) { Hoisted += Hoister(Loop, PureNames).run(); });
    if (Changed)
      *Changed += Hoisted;
  }

  ChangeCounter *Changed = nullptr;

  ListOption<std::string> Pure{
      *this, "pure",
      llvm::cl::desc("Functions that have no effects and always return")};
};

} // namespace

std::unique_ptr<mlir::Pass>
createLICMPass(llvm::ArrayRef<std::string> PureFunctions,
               ChangeCounter *Changed) {
  return std::make_unique<LICMPass>(PureFunctions, Changed);
}

void registerLICMPass() { mlir::PassRegistration<LICMPass>(); }

} // namespace optimization
