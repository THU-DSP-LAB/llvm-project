// RUN: not llvm-tblgen -gen-asm-writer -I %p -I %p/../../include %s 2>&1 | FileCheck %s -DFILE=%s

include "ConstraintChecking.inc"

// CHECK: [[FILE]]:[[@LINE+1]]:5: error: Illegal format custom number in range[0, 15] for custom constraint: '$dest1 = 16'
def Foo : TestInstructionWithConstraints<"", "$dest1 = 16">;
