#include "Passes.hpp"

namespace optimization {

// Defined next to each pass, which is local to its file.
void registerUnrollPass();
void registerTilingPass();
void registerDCEPipeline();
void registerInlinePass();
void registerCanonicalizePipeline();
void registerTestCanonicalLoopsPass();

void registerSmrPasses() {
  registerUnrollPass();
  registerTilingPass();
  registerDCEPipeline();
  registerInlinePass();
  registerCanonicalizePipeline();
  registerTestCanonicalLoopsPass();
}

} // namespace optimization
