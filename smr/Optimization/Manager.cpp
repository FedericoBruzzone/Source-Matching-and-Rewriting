#include "Manager.hpp"
#include "Logger/Logger.hpp"
#include "Logger/Messages.hpp"
#include "Passes.hpp"
#include "mlir/Pass/PassManager.h"

namespace optimization {

/// Reads the single integer argument of Spec, or Default when it has none.
static std::optional<unsigned> getUnsignedArg(const OptimizationSpec &Spec,
                                              unsigned Default) {
  if (Spec.Args.empty())
    return Default;
  unsigned Value;
  if (llvm::StringRef(Spec.Args.front()).getAsInteger(10, Value) ||
      Value == 0) {
    error(Msg::INVALID_OPTIMIZATION_ARG, Optimization::getName(Spec.Code),
          Spec.Args.front());
    return std::nullopt;
  }
  return Value;
}

int Manager::run(mlir::ModuleOp Module,
                 const std::vector<OptimizationSpec> &Opts) {
  // Implicit nesting runs the function passes on every cir.func.
  mlir::PassManager PM(Module.getContext(), mlir::ModuleOp::getOperationName(),
                       mlir::OpPassManager::Nesting::Implicit);

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

    switch (OptSpec.Code) {
    case UNROLL: {
      auto Factor = getUnsignedArg(OptSpec, 4);
      if (!Factor)
        return 1;
      PM.addPass(createUnrollPass(*Factor));
      break;
    }
    case TILING: {
      auto Size = getUnsignedArg(OptSpec, 32);
      if (!Size)
        return 1;
      PM.addPass(createTilingPass(*Size));
      break;
    }
    case DCE:
      PM.addPass(createDCEPass());
      break;
    case INLINE:
      PM.addPass(createInlinePass());
      break;
    case VECTORIZE:
    case UNKNOWN:
      warn(Msg::OPTIMIZATION_NOT_IMPLEMENTED, OptName);
      break;
    }
  }

  if (mlir::failed(PM.run(Module))) {
    error(Msg::OPTIMIZATION_FAILED);
    return 1;
  }
  return 0;
}

} // namespace optimization
