#include "Passes.hpp"
#include "clang/CIR/Dialect/Passes.h"

namespace optimization {

void addCanonicalizePasses(mlir::OpPassManager &PM) {
  PM.addPass(mlir::createCIRCanonicalizePass());
  PM.addPass(mlir::createCIRSimplifyPass());
}

void registerCanonicalizePipeline() {
  mlir::PassPipelineRegistration<>("smr-canonicalize",
                                   "Run cir-canonicalize, then cir-simplify",
                                   addCanonicalizePasses);
}

} // namespace optimization
