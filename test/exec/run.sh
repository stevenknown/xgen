#Run single case.
perl ./run.pl arm XoccPath = ~/xgen/arm/xocc.exe Case = hello.c XoccFlag = "-O3"

#Run all testcases, only compile.
perl ./run.pl arm XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NoAsm

#Run all testcases, only compile, assembly.
perl ./run.pl arm XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NoLink

#Run all testcases, only compile, assembly, linking.
perl ./run.pl arm XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NoRun

#Run all testcases, compile, assembly, linking, and run on simulator.
perl ./run.pl arm XoccPath = ~/xgen/arm/xocc.exe XoccFlag = "-O3" NotQuitEarly
