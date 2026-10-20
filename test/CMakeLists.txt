# llvm-lit and FileCheck are not installed with LLVM by default: look next to
# the LLVM tools first, then in the ClangIR build tree of the dev image.
set(SMR_LLVM_TOOLS_HINTS ${LLVM_TOOLS_BINARY_DIR} /opt/llvm-project/build/bin)
find_program(SMR_LIT NAMES llvm-lit lit HINTS ${SMR_LLVM_TOOLS_HINTS})
find_program(SMR_FILECHECK NAMES FileCheck HINTS ${SMR_LLVM_TOOLS_HINTS})

if (NOT EXISTS "${SMR_LIT}" OR NOT EXISTS "${SMR_FILECHECK}")
  message(WARNING "llvm-lit or FileCheck not found, check-smr is disabled. "
                  "Set SMR_LIT and SMR_FILECHECK to enable it.")
  return()
endif()

get_filename_component(SMR_FILECHECK_DIR ${SMR_FILECHECK} DIRECTORY)

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

set(LLVM_EXTERNAL_LIT ${SMR_LIT})
add_lit_testsuite(check-smr "Running the SMR tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS smr smr-opt
)
