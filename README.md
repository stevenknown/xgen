XGEN, a retargetable code generator for risc, cisc, vliw machine.

It uses XOC IR as the input representation, and convert XOC IR to XGEN's OR and SR.

XGEN includes IR2OR convertion, register allocation, instruction scheduling, peephole optimizations, target dependent loop unrolling, instruction packaging, and assembly file print.

Contribution and License Agreement. If you contribute code to this project, you are implicitly allowing your code to be distributed under the BSD license.

usage:
 1. clone xoc project     
```cmd
    git clone https://github.com/stevenknown/xoc.git
```
 2. copy com, reader, and opt into xgen directory     
```cmd
    cd xgen    
    cp -rf ../xoc/com ../xoc/opt ../xoc/reader ../xoc/Makefile.xoc ../xoc/Makefile.xoc.inc .    
```
 3. clone xocfe project    
```cmd
    git clone https://github.com/stevenknown/xocfe.git
```
 4. copy xocfe/src/cfe into xgen directory       
```cmd
    cd xgen    
    cp -rf ../xocfe/src/cfe .    
```
 5. Build xocc
```cmd
    cd xgen/arm
    make -f Makefile.xocc    
```
 6. Install arm-assembler, arm-linker:    
```cmd
    sudo apt-get install gcc-arm-linux-gnueabihf    
```
 7. Install qemu-arm:    
```cmd
    sudo apt-get install qemu-user-static       
```
 8. Test xocc.exe       
    write hello.c       
```cmd
    xocc.exe hello.c -o hello.s
    arm-linux-gnueabihf-as hello.s -o hello.o
    arm-linux-gnueabihf-gcc hello.o -o hello.out
    qemu-arm -L /usr/arm-linux-gnueabihf hello.out
```
 9. cd xgen/test and there are a lot of testfiles wrote in C or GR language.      
    e.g: Run testcases in 'exec':     
```cmd
    cd exec     
    perl ./run.pl      
```
    It will show you all commandlines provided by run.pl,    
    Try and see the simplest test:    
```cmd
    perl ./run.pl Targ = arm      
```
    You can see all C files under 'exec' will be compiled by xocc.exe and running on qemu-arm.    
    e.g2: Run single case in 'exec':    
```cmd
    perl ./run.pl Targ = arm Case = shift.c    
```
