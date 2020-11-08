#Run single case.
#perl ./run.pl arm OverrideXoccPath = ~/xgen/arm/xocc.exe Case = block.c NoAsm OverrideXoccFlag = "-O3 -time"

#Run all testcases.
perl ./run.pl arm OverrideXoccPath = ~/xgen/arm/xocc.exe OverrideXoccFlag = "-O3" NoAsm
