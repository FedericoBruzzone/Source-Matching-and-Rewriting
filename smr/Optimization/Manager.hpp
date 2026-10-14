#pragma once

#include "Optimization.hpp"
#include "mlir/IR/BuiltinOps.h"
#include <vector>

namespace optimization {

/// Manager responsible for executing transformation pipelines on MLIR modules.
class Manager {
public:
  /// Runs registered optimization passes on the target MLIR module.
  static int run(mlir::ModuleOp Module,
                 const std::vector<OptimizationSpec> &Opts);
};

} // namespace optimization