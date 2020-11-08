#Run single case.
#perl ./run.pl arm OverrideXoccPath = ~/xgen/arm/xocc.exe Case = array_and_compute_sensitive_code.pr_mode.gr NoAsm OverrideXoccFlag = "-O3 -time"

#Run all testcases.
perl ./run.pl arm OverrideXoccPath = ~/xgen/arm/xocc.exe OverrideXoccFlag = "-O3" NoAsm
