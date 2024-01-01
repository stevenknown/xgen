#SET xocc.exe path here
xocc_path="~/xocc/xocc.exe"

### BUILD BUINTIN ###
perl run.pl Dir = builtin Targ = armhf XoccPath = $xocc_path XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist LinkerFlag = "-lc -lm -lgcc -lm -lc" NoLink
