xocc_path="../../src/xocc/xocc.exe"
### RUN SINGLE CASE ###
#perl run.pl Case = array_alias2.c Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-include_region = "main" -O3 -only-dce -dce -rce -gvn -cp -no-vect -no-lsra -dump-all -dump tmp.log " \
#  LinkerFlag = " " CompareDumpIfExist CompareResultIfExist

### BUILD ALL CASES ###
perl run.pl Targ = armhf XoccPath = $xocc_path \
  XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist CompareResultIfExist\
  LinkerFlag = " "
