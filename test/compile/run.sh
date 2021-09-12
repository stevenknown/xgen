#Run single case.
#perl ./run.pl arm Case = block.c XoccPath = ~/xgen/arm/xocc.exe NoAsm XoccFlag = "-O3 -time" NoAsm
#perl ./run.pl arm Case = zlib-5.c Dir = analyzer XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NoAsm

#Run all testcases.
#perl ./run.pl arm XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NoAsm

#Run testcases under given directory
#perl ./run.pl arm Case = alias10.c Dir = analyzer XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NoAsm
perl ./run.pl arm Case = alias10.c Dir = analyzer XoccPath = ../../src/xocc.for.arm/x64/Debug/xocc.for.arm.exe XoccFlag = "-O3" NoAsm

