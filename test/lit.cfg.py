import os

import lit.formats

config.name = "SMR"
config.test_format = lit.formats.ShTest(execute_external=False)
config.suffixes = [".c", ".f90", ".mlir", ".test"]

# Inputs holds the files tests read (patterns, headers), not tests.
config.excludes = ["Inputs", "CMakeLists.txt", "lit.cfg.py", "lit.site.cfg.py", "cello"]

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.smr_obj_root, "test")

# smr compiles inputs by calling clang and flang, so the LLVM tools must be on
# PATH too, after the freshly built smr.
config.environment["PATH"] = os.pathsep.join(
    [
        config.smr_tools_dir,
        config.llvm_tools_dir,
        config.filecheck_dir,
        config.environment["PATH"],
    ]
)
