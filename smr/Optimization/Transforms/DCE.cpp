#include "Passes.hpp"
#include "mlir/Transforms/Passes.h"

namespace optimization {

void addDCEPasses(mlir::OpPassManager &PM) {
  PM.addPass(mlir::createCanonicalizerPass());
  PM.addPass(mlir::createSymbolDCEPass());
}

void registerDCEPipeline() {
  mlir::PassPipelineRegistration<>(
      "smr-dce", "Erase dead operations and unused private functions",
      addDCEPasses);
}

} // namespace optimization
