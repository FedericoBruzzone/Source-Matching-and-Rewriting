#include "Analysis/CanonicalLoop.hpp"
#include "Passes.hpp"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

namespace optimization {
namespace {

/// A and B are computed the same way: the same value, the same constants, the
/// same slot loaded, or the same pure operation on operands that match.
bool sameValue(mlir::Value A, mlir::Value B) {
  if (A == B)
    return true;
  mlir::Operation *OpA = A.getDefiningOp(), *OpB = B.getDefiningOp();
  if (!OpA || !OpB || OpA->getName() != OpB->getName() ||
      OpA->getAttrDictionary() != OpB->getAttrDictionary() ||
      OpA->getResultTypes() != OpB->getResultTypes() ||
      OpA->getNumOperands() != OpB->getNumOperands())
    return false;
  if (auto Load = mlir::dyn_cast<cir::LoadOp>(OpA))
    return Load.getAddr() == mlir::cast<cir::LoadOp>(OpB).getAddr();
  if (!mlir::isPure(OpA))
    return false;
  return llvm::all_of(llvm::zip_equal(OpA->getOperands(), OpB->getOperands()),
                      [](auto Pair) {
                        return sameValue(std::get<0>(Pair), std::get<1>(Pair));
                      });
}

bool sameConstant(mlir::Value A, mlir::Value B) {
  auto ConstA = A.getDefiningOp<cir::ConstantOp>();
  auto ConstB = B.getDefiningOp<cir::ConstantOp>();
  return ConstA && ConstB && ConstA.getValue() == ConstB.getValue();
}

struct Pair {
  CanonicalLoop First, Second;
};

/// First and the loop right after it, if they run the same iterations. Only
/// constants and the write of the IV of the second loop may sit between them.
/// Whether the bodies can share an iteration is not checked, the pattern says
/// so.
std::optional<Pair> matchPair(cir::ForOp First) {
  auto A = matchCanonicalLoop(First);
  if (!A || !First.getBody().hasOneBlock())
    return std::nullopt;

  cir::ForOp Next;
  for (mlir::Operation *Op = First->getNextNode(); Op; Op = Op->getNextNode()) {
    if ((Next = mlir::dyn_cast<cir::ForOp>(Op)))
      break;
    if (!mlir::isa<cir::ConstantOp, cir::StoreOp>(Op))
      return std::nullopt;
  }
  if (!Next || !Next.getBody().hasOneBlock())
    return std::nullopt;
  auto B = matchCanonicalLoop(Next);
  if (!B || !A->LowerBound || !B->LowerBound ||
      !sameConstant(*A->LowerBound, *B->LowerBound) || A->Step != B->Step ||
      A->Pred != B->Pred || !sameValue(A->UpperBound, B->UpperBound))
    return std::nullopt;

  for (mlir::Operation *Op = First->getNextNode(); Op != Next.getOperation();
       Op = Op->getNextNode())
    if (auto Store = mlir::dyn_cast<cir::StoreOp>(Op);
        Store && Store.getAddr() != B->IV.getResult())
      return std::nullopt;

  // The second IV disappears with its loop, so nothing else may read it.
  if (A->IV != B->IV)
    for (mlir::Operation *User : B->IV->getUsers())
      if (!Next->isAncestor(User) && User->getParentOp() != First->getParentOp())
        return std::nullopt;
  return Pair{*A, *B};
}

/// Runs the body of the second loop after the body of the first, in the same
/// iteration, and drops the second loop:
///
///   for (; i < n; ++i) A      becomes     for (; i < n; ++i) { A  B }
///   for (; j < n; ++j) B
///
/// B reads the IV of the first loop where it read its own.
void fuse(Pair P) {
  cir::ForOp First = P.First.Loop, Second = P.Second.Loop;
  mlir::IRRewriter Rewriter(First.getContext());

  // The constants stay above the first loop, which now holds the second body.
  for (mlir::Operation *Op = First->getNextNode(); Op != Second.getOperation();) {
    mlir::Operation *Next = Op->getNextNode();
    if (mlir::isa<cir::ConstantOp>(Op))
      Op->moveBefore(First);
    else
      Rewriter.eraseOp(Op);
    Op = Next;
  }

  mlir::IRMapping Mapping;
  Mapping.map(mlir::Value(P.Second.IV.getResult()),
              mlir::Value(P.First.IV.getResult()));
  Rewriter.setInsertionPoint(First.getBody().front().getTerminator());
  for (mlir::Operation &Op : Second.getBody().front().without_terminator())
    Rewriter.clone(Op, Mapping);
  Rewriter.eraseOp(Second);
}

/// `for (int i = 0; ...)` puts the loop and its IV in a scope of their own, so
/// two loops in a row are not neighbours. Hoists the allocas of those scopes to
/// the entry block and splices the scopes into their parents.
void flattenScopes(cir::FuncOp Func) {
  mlir::Block &Entry = Func.getBody().front();
  llvm::SmallVector<cir::ScopeOp> Scopes;
  Func.walk([&](cir::ScopeOp Scope) {
    if (Scope->getNumResults() != 0 || !Scope.getScopeRegion().hasOneBlock())
      return;
    mlir::Block &Block = Scope.getScopeRegion().front();
    auto Yield = mlir::dyn_cast<cir::YieldOp>(Block.getTerminator());
    bool HasLoop = !Block.getOps<cir::ForOp>().empty();
    bool HasDynamicAlloca = llvm::any_of(
        Block.getOps<cir::AllocaOp>(),
        [](cir::AllocaOp Alloca) { return Alloca.getDynAllocSize(); });
    if (Yield && Yield->getNumOperands() == 0 && HasLoop && !HasDynamicAlloca)
      Scopes.push_back(Scope);
  });
  for (cir::ScopeOp Scope : Scopes) {
    mlir::Block &Block = Scope.getScopeRegion().front();
    llvm::SmallVector<cir::AllocaOp> Allocas(Block.getOps<cir::AllocaOp>());
    for (cir::AllocaOp Alloca : Allocas)
      Alloca->moveBefore(&Entry, Entry.begin());
    Block.getTerminator()->erase();
    Scope->getBlock()->getOperations().splice(Scope->getIterator(),
                                              Block.getOperations());
    Scope->erase();
  }
}

struct FusionPass
    : public mlir::PassWrapper<FusionPass, mlir::OperationPass<cir::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FusionPass)

  FusionPass() = default;
  FusionPass(const FusionPass &Other) : PassWrapper(Other), Changed(Other.Changed) {}
  explicit FusionPass(ChangeCounter *Counter) : Changed(Counter) {}

  llvm::StringRef getArgument() const final { return "smr-fusion"; }
  llvm::StringRef getDescription() const final {
    return "Fuse adjacent canonical cir.for loops that run the same iterations";
  }

  void runOnOperation() override {
    if (getOperation().getBody().empty())
      return;
    flattenScopes(getOperation());

    unsigned Fused = 0;
    while (true) {
      std::optional<Pair> Found;
      getOperation().walk([&](cir::ForOp For) {
        Found = matchPair(For);
        return Found ? mlir::WalkResult::interrupt()
                     : mlir::WalkResult::advance();
      });
      if (!Found)
        break;
      fuse(*Found);
      ++Fused;
    }
    if (Changed)
      *Changed += Fused;
  }

  ChangeCounter *Changed = nullptr;
};

} // namespace

std::unique_ptr<mlir::Pass> createFusionPass(ChangeCounter *Changed) {
  return std::make_unique<FusionPass>(Changed);
}

void registerFusionPass() { mlir::PassRegistration<FusionPass>(); }

} // namespace optimization
