#Run single case.
#perl ./run.pl arm Case = block.c OverrideXoccPath = ~/xgen/arm/xocc.exe NoAsm OverrideXoccFlag = "-O3 -time" NoAsm
perl ./run.pl arm Case = zlib-5.c Dir = analyzer OverrideXoccPath = ~/xgen/arm/xocc.exe OverrideXoccFlag = "-O3" NoAsm

#Run all testcases.
perl ./run.pl arm OverrideXoccPath = ~/xgen/arm/xocc.exe OverrideXoccFlag = "-O3" NoAsm

#Run testcases under given directory
perl ./run.pl arm Dir = analyzer OverrideXoccPath = ~/xgen/arm/xocc.exe OverrideXoccFlag = "-O3" NoAsm

