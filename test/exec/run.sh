xocc_path="../../src/xocc/xocc.exe"
perl run.pl Targ = armhf XoccPath = $xocc_path XoccFlag = "-O0" CompareDumpIfExist LinkerFlag = "-lc -lm -lgcc -lm -lc"
