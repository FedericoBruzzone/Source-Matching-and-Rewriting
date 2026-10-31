#include "Analysis/CanonicalLoop.hpp"
#include "Passes.hpp"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"

namespace optimization {
namespace {

/// Strip-mines Loop in tiles of Size iterations:
///
///   for (; iv < ub; iv += s) body
///
/// becomes
///
///   for (; iv < ub;) {
///     n = 0;
///     for (; iv < ub && n < Size; iv += s, ++n) body
///   }
///
/// The IV lives in memory, so every tile continues where the previous one
/// stopped. Counting the iterations of a tile in n, instead of bounding the IV
/// by iv + Size * s, cannot overflow.
void stripMine(CanonicalLoop Loop, unsigned Size) {
  cir::ForOp Inner = Loop.Loop;
  mlir::IRRewriter Rewriter(Inner.getContext());
  mlir::Location Loc = Inner.getLoc();
  auto CountType = cir::IntType::get(Inner.getContext(), 64, false);
  auto Constant = [&](int64_t Value) {
    return cir::ConstantOp::create(Rewriter, Loc,
                                   cir::IntAttr::get(CountType, Value));
  };

  cir::FuncOp Func = Inner->getParentOfType<cir::FuncOp>();
  Rewriter.setInsertionPointToStart(&Func.getBody().front());
  auto Count =
      cir::AllocaOp::create(Rewriter, Loc, cir::PointerType::get(CountType),
                            CountType, "tile", Rewriter.getI64IntegerAttr(8));

  Rewriter.setInsertionPoint(Inner);
  auto Outer = cir::ForOp::create(
      Rewriter, Loc,
      [&](mlir::OpBuilder &Builder, mlir::Location) {
        mlir::IRMapping Mapping;
        for (mlir::Operation &Op : Inner.getCond().front())
          Builder.clone(Op, Mapping);
      },
      [&](mlir::OpBuilder &Builder, mlir::Location Loc) {
        auto Zero = cir::ConstantOp::create(Builder, Loc,
                                            cir::IntAttr::get(CountType, 0));
        cir::StoreOp::create(Builder, Loc, Zero, Count);
        cir::YieldOp::create(Builder, Loc);
      },
      [&](mlir::OpBuilder &Builder, mlir::Location Loc) {
        cir::YieldOp::create(Builder, Loc);
      });
  Inner->moveBefore(Outer.getBody().front().getTerminator());

  // Cond: also stop when the tile is full.
  auto Condition =
      mlir::cast<cir::ConditionOp>(Inner.getCond().front().getTerminator());
  Rewriter.setInsertionPoint(Condition);
  auto Done = cir::LoadOp::create(Rewriter, Loc, CountType, Count.getResult());
  auto Room =
      cir::CmpOp::create(Rewriter, Loc, cir::BoolType::get(Inner.getContext()),
                         cir::CmpOpKind::lt, Done, Constant(Size));
  auto False = cir::ConstantOp::create(
      Rewriter, Loc, cir::BoolAttr::get(Inner.getContext(), false));
  auto Both = cir::SelectOp::create(Rewriter, Loc, Condition.getCondition(),
                                    Room, False);
  Rewriter.modifyOpInPlace(
      Condition, [&] { Condition.getConditionMutable().assign(Both); });

  // Step: ++n.
  mlir::Operation *Yield = Inner.getStep().front().getTerminator();
  Rewriter.setInsertionPoint(Yield);
  auto Taken = cir::LoadOp::create(Rewriter, Loc, CountType, Count.getResult());
  auto Next = cir::BinOp::create(Rewriter, Loc, CountType, cir::BinOpKind::Add,
                                 Taken, Constant(1));
  cir::StoreOp::create(Rewriter, Loc, Next, Count);
}

struct TilingPass
    : public mlir::PassWrapper<TilingPass, mlir::OperationPass<cir::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(TilingPass)

  TilingPass() = default;
  TilingPass(const TilingPass &Other) : PassWrapper(Other) {}
  explicit TilingPass(unsigned SizeValue) { Size = SizeValue; }

  llvm::StringRef getArgument() const final { return "smr-tiling"; }
  llvm::StringRef getDescription() const final {
    return "Strip-mine innermost canonical cir.for loops";
  }

  void runOnOperation() override {
    if (Size <= 1)
      return;

    for (CanonicalLoop &Loop : getInnermostCanonicalLoops(getOperation()))
      if (!Loop.TripCount || *Loop.TripCount > Size)
        stripMine(Loop, Size);
  }

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
