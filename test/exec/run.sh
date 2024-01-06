xocc_path="../../src/xocc/xocc.exe"
### RUN SINGLE CASE ###
#perl run.pl Case = array_alias2.c Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-include_region = "main" -O3 -only-dce -dce -rce -gvn -cp -no-vect -no-lsra -dump-all -dump tmp.log " \
#  LinkerFlag = " builtin/_clz32.c.i.o builtin/_clz64.c.i.o \
#    builtin/_divdi3.c.i.o builtin/_moddi3.c.i.o builtin/udivmodsi4.c.i.o \
#    builtin/sqrtf.c.i.o builtin/fabsf.c.i.o " CompareDumpIfExist
#    builtin/sqrt.c.i.o builtin/fabs.c.i.o " CompareDumpIfExist

### BUILD ALL CASES ###
perl run.pl Targ = armhf XoccPath = $xocc_path \
  XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist CompareResultIfExist\
  LinkerFlag = " "
