#include "Passes.hpp"

namespace optimization {

// Defined next to each pass, which is local to its file.
void registerUnrollPass();
void registerTilingPass();
void registerDCEPass();
void registerInlinePass();

void registerSmrPasses() {
  registerUnrollPass();
  registerTilingPass();
  registerDCEPass();
  registerInlinePass();
}

} // namespace optimization
