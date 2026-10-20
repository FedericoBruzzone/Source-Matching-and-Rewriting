#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"

int main(int argc, char **argv) {
  // The modules smr emits carry DLTI and LLVM attributes next to the CIR.
  mlir::DialectRegistry Registry;
  Registry.insert<cir::CIRDialect, mlir::arith::ArithDialect,
                  mlir::func::FuncDialect, mlir::DLTIDialect,
                  mlir::LLVM::LLVMDialect>();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "SMR optimization driver\n", Registry));
}
