#include "Manager.hpp"
#include "Logger/Logger.hpp"
#include "Logger/Messages.hpp"

namespace optimization {

int Manager::run(mlir::ModuleOp Module,
                 const std::vector<OptimizationSpec> &Opts) {
  for (const auto &OptSpec : Opts) {
    std::string OptName = Optimization::getName(OptSpec.Code);

    // Format arguments if present: e.g., unroll(3) or tiling(32)
    if (!OptSpec.Args.empty()) {
      OptName += "(";
      for (size_t i = 0; i < OptSpec.Args.size(); ++i) {
        if (i > 0)
          OptName += ", ";
        OptName += OptSpec.Args[i];
      }
      OptName += ")";
    }

    // Print optimization status using the SMR logging framework
    info(Msg::APPLYING_OPTIMIZATION, OptName);

    // Future work: invoke corresponding MLIR pass pipelines
  }
  return 0;
}

} // namespace optimization