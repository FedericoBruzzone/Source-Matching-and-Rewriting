#pragma once

#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include <memory>

namespace optimization {

/// Unrolls the innermost cir.for loops of a function by Factor.
std::unique_ptr<mlir::Pass> createUnrollPass(unsigned Factor);

/// Tiles the cir.for loops of a function with tiles of Size iterations.
std::unique_ptr<mlir::Pass> createTilingPass(unsigned Size);

/// Erases dead operations and the private functions nobody calls.
std::unique_ptr<mlir::Pass> createDCEPass();

/// Inlines the calls to functions defined in the module.
std::unique_ptr<mlir::Pass> createInlinePass();

/// Adds CIR's own cleanups: cir-canonicalize, then cir-simplify.
void addCanonicalizePasses(mlir::OpPassManager &PM);

/// Registers the passes above as smr-opt flags (--smr-unroll, ...).
/// addCanonicalizePasses is the --smr-canonicalize pipeline.
void registerSmrPasses();

} // namespace optimization
