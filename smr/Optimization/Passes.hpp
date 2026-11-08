#pragma once

#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "llvm/ADT/ArrayRef.h"
#include <memory>
#include <string>

namespace optimization {

/// Unrolls the innermost cir.for loops of a function by Factor.
std::unique_ptr<mlir::Pass> createUnrollPass(unsigned Factor);

/// Tiles the cir.for loops of a function with tiles of Size iterations.
std::unique_ptr<mlir::Pass> createTilingPass(unsigned Size);

/// Hoists loop invariant code out of the cir.for loops of a function. Calls to
/// the functions in PureFunctions are hoisted too.
std::unique_ptr<mlir::Pass>
createLICMPass(llvm::ArrayRef<std::string> PureFunctions = {});

/// Adds the passes of dce.
void addDCEPasses(mlir::OpPassManager &PM);

/// Inlines the calls to functions defined in the module.
std::unique_ptr<mlir::Pass> createInlinePass();

/// Adds CIR's own cleanups: cir-canonicalize, then cir-simplify.
void addCanonicalizePasses(mlir::OpPassManager &PM);

/// Registers the passes above as smr-opt flags (--smr-unroll, ...).
/// addCanonicalizePasses is the --smr-canonicalize pipeline.
void registerSmrPasses();

} // namespace optimization
