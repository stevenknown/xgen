XGEN, a retargetable code generator for risc, cisc, vliw machine.

It uses XOC IR as the input representation, and convert XOC IR to XGEN's OR and SR.

XGEN includes IR2OR convertion, register allocatin, instruction scheduling, peephole optimizations, target dependent loop unrolling, instruction package, and assembly file print.

Contribution and License Agreement. If you contribute code to this project, you are implicitly allowing your code to be distributed under the BSD license.

usage:
 1. clone xoc project
 2. copy com, reader, opt xpoly into xgen directory
    cp -rf ../xoc/com ../xoc/opt ../xoc/reader .
 3. clone xocfe project
 4. copy xocfe/src/cfe into xgen directory
 5. cd xgen/arm directory
 6. make -f Make.xocc
