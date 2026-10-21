#include "Passes.hpp"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "llvm/ADT/SmallVector.h"

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
    return "Unroll innermost cir.for loops";
  }

  void runOnOperation() override {
    cir::FuncOp func = getOperation();
    if (Factor <= 1)
      return;

    // Collect all innermost cir.for loops to avoid iterator invalidation
    llvm::SmallVector<cir::ForOp> innermostLoops;

    func.walk([&](cir::ForOp forOp) {
      bool hasChildLoop = false;
      forOp.walk([&](cir::ForOp childFor) {
        if (childFor != forOp)
          hasChildLoop = true;
      });

      if (!hasChildLoop)
        innermostLoops.push_back(forOp);
    });

    mlir::IRRewriter rewriter(&getContext());

    for (auto forOp : innermostLoops) {
      if (forOp.getBody().empty())
        continue;
      mlir::Block *body = &forOp.getBody().front();
      if (body->empty())
        continue;

      mlir::Location loc = forOp.getLoc();
      auto intTy = cir::IntType::get(&getContext(), 32, /*isSigned=*/true);

      // PREPARE CONSTANTS BEFORE LOOP TO SATISFY SSA DOMINANCE
      rewriter.setInsertionPoint(forOp);

      mlir::Value ivPtr = nullptr;
      mlir::Value originalStepVal = nullptr;

      // Inspect step block to find IV pointer and original step value
      if (!forOp.getStep().empty()) {
        mlir::Block *stepBlock = &forOp.getStep().front();
        for (auto &op : *stepBlock) {
          if (auto loadOp = llvm::dyn_cast<cir::LoadOp>(&op)) {
            ivPtr = loadOp.getAddr();
          } else if (auto binOp = llvm::dyn_cast<cir::BinOp>(&op)) {
            mlir::Value rhs = binOp.getRhs();
            // If RHS is defined inside forOp, clone defining op before forOp
            if (mlir::Operation *defOp = rhs.getDefiningOp()) {
              if (forOp->isAncestor(defOp)) {
                originalStepVal = rewriter.clone(*defOp)->getResult(0);
              } else {
                originalStepVal = rhs;
              }
            } else {
              originalStepVal = rhs;
            }
          }
        }
      }

      // Default step fallback to 1 if step is unary increment (e.g., i++) or if step is not found
      if (!originalStepVal) {
        auto oneAttr = cir::IntAttr::get(intTy, llvm::APInt(32, 1, /*isSigned=*/true));
        originalStepVal = cir::ConstantOp::create(rewriter, loc, oneAttr);
      }

      // Create factor constant before forOp
      auto factorAttr = cir::IntAttr::get(intTy, llvm::APInt(32, Factor, /*isSigned=*/true));
      auto factorConst = cir::ConstantOp::create(rewriter, loc, factorAttr);

      // Compute total chunk stride (Factor * Step) before forOp to guarantee dominance
      mlir::Value chunkStride = cir::BinOp::create(
          rewriter, loc, intTy, cir::BinOpKind::Mul, factorConst, originalStepVal);

      // CLONE ORIGINAL LOOP TO CREATE EPILOGUE LOOP
      rewriter.setInsertionPointAfter(forOp);
      auto epilogueForOp = llvm::cast<cir::ForOp>(rewriter.clone(*forOp.getOperation()));

      // DUPLICATE MAIN LOOP BODY WITH OFFSET (i * Step)
      mlir::Operation *terminator = body->getTerminator();
      llvm::SmallVector<mlir::Operation *> opsToClone;
      for (auto &op : *body) {
        if (&op != terminator) {
          opsToClone.push_back(&op);
        }
      }

      for (unsigned i = 1; i < Factor; ++i) {
        rewriter.setInsertionPoint(terminator);
        mlir::IRMapping mapper;

        // Compute copy offset: Offset_i = i * originalStepVal
        auto iAttr = cir::IntAttr::get(intTy, llvm::APInt(32, i, /*isSigned=*/true));
        auto iConst = cir::ConstantOp::create(rewriter, loc, iAttr);
        mlir::Value stepOffset = cir::BinOp::create(
            rewriter, loc, intTy, cir::BinOpKind::Mul, iConst, originalStepVal);

        // Map SSA block argument if present
        if (body->getNumArguments() > 0) {
          mlir::Value iv = body->getArgument(0);
          auto offsetIv = cir::BinOp::create(
              rewriter, loc, iv.getType(), cir::BinOpKind::Add, iv, stepOffset);
          mapper.map(iv, offsetIv.getResult());
        }

        // Clone body operations and offset loads from IV
        for (auto *op : opsToClone) {
          mlir::Operation *clonedOp = rewriter.clone(*op, mapper);

          if (ivPtr) {
            if (auto loadOp = llvm::dyn_cast<cir::LoadOp>(clonedOp)) {
              if (loadOp.getAddr() == ivPtr ||
                  (mapper.contains(ivPtr) && loadOp.getAddr() == mapper.lookup(ivPtr))) {
                rewriter.setInsertionPointAfter(clonedOp);
                auto offsetVal = cir::BinOp::create(
                    rewriter, loc, loadOp.getType(), cir::BinOpKind::Add, loadOp.getResult(), stepOffset);

                mapper.map(op->getResult(0), offsetVal.getResult());
              }
            }
          }
        }
      }

      // UPDATE STEP & BOUNDS IN MAIN LOOP

      // Update Step Region in Main Loop (NewStep = ChunkStride)
      if (!forOp.getStep().empty()) {
        mlir::Block *stepBlock = &forOp.getStep().front();
        for (auto &op : llvm::make_early_inc_range(*stepBlock)) {
          if (auto binOp = llvm::dyn_cast<cir::BinOp>(&op)) {
            rewriter.setInsertionPoint(&op);
            binOp.setOperand(1, chunkStride);
          } else if (auto unaryOp = llvm::dyn_cast<cir::UnaryOp>(&op)) {
            rewriter.setInsertionPoint(&op);
            auto scaledAdd = cir::BinOp::create(
                rewriter, loc, unaryOp.getType(), cir::BinOpKind::Add, unaryOp.getInput(), chunkStride);
            rewriter.replaceOp(&op, scaledAdd.getOperation());
          }
        }
      }

      // Adjust Upper Bound in Main Loop Condition (MainLimit = Upper - Remainder)
      if (!forOp.getCond().empty()) {
        mlir::Block *condBlock = &forOp.getCond().front();
        for (auto &op : *condBlock) {
          if (auto cmpOp = llvm::dyn_cast<cir::CmpOp>(&op)) {
            rewriter.setInsertionPoint(&op);

            mlir::Value originalUpperBound = cmpOp.getRhs();
            mlir::Value lowerBound = cmpOp.getLhs();

            mlir::Value range = cir::BinOp::create(
                rewriter, loc, intTy, cir::BinOpKind::Sub, originalUpperBound, lowerBound);

            mlir::Value remainder = cir::BinOp::create(
                rewriter, loc, intTy, cir::BinOpKind::Rem, range, chunkStride);

            mlir::Value mainLimit = cir::BinOp::create(
                rewriter, loc, intTy, cir::BinOpKind::Sub, originalUpperBound, remainder);

            cmpOp.setOperand(1, mainLimit);
          }
        }
      }

      (void)epilogueForOp;
    }
  }

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
