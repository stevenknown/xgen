debug_xocc_path="/home/zhenyu/x/src/arm/xocc.exe"
release_xocc_path="/home/zhenyu/x/src/arm/xocc.exe"

perl ./run.pl armhf Case = pi2.c XoccPath = $debug_xocc_path XoccFlag = "-O0" CompareResultIfExist
#NotQuitEarly
