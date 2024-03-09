xocc_path="~/xocc/xocc.exe"
#XoccFlag = "-O3 -cp -gvn -rce -no-vect -no-lsra -include_region = "test_qsort1" " \

### OPTIONS
#NotQuitEarly 

### RUN SINGLE CASE ###
#perl run.pl Case = rem.c Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-O3 -no-vect -no-lsra -only-dce -cp -gvn -cfgopt -dce_aggr -dump-all -dump tmp.log " \
#  LinkerFlag = " builtin/_clz32.c.i.o builtin/_clz64.c.i.o \
#    builtin/_divdi3.c.i.o builtin/_moddi3.c.i.o builtin/udivmodsi4.c.i.o \
#    builtin/sqrtf.c.i.o builtin/fabsf.c.i.o " CompareDumpIfExist
#    builtin/sqrt.c.i.o builtin/fabs.c.i.o " CompareDumpIfExist

#XoccFlag = "-O3 -only-rp -rp -gvn -no-vect -no-lsra -dump-all -dump tmp.log -include_region = "sort""
#perl run.pl Case = sign_shift2.c Targ = armhf XoccPath = $xocc_path XoccFlag = "-O0 -no-vect -no-lsra" LinkerFlag = "builtin/sqrt.c.i.o builtin/fabs.c.i.o " CompareDumpIfExist CompareResultIfExist
#perl run.pl Case = test_shift.c Targ = armhf XoccPath = $xocc_path XoccFlag = "-O0 -no-vect -no-lsra" LinkerFlag = "builtin/sqrt.c.i.o builtin/fabs.c.i.o " CompareDumpIfExist CompareResultIfExist
  

### BUILD ALL CASES ###
#builtin/divmod.c.i.o builtin/_umoddi3.c.i.o builtin/_udivdi3.c.i.o 
#perl run.pl NotQuitEarly Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist \
#  LinkerFlag = " builtin/_clz32.c.i.o builtin/_clz64.c.i.o builtin/_divdi3.c.i.o builtin/_moddi3.c.i.o builtin/udivmodsi4.c.i.o builtin/sqrtf.c.i.o builtin/fabsf.c.i.o "
perl run.pl Targ = armhf XoccPath = $xocc_path \
  XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist CompareResultIfExist\
  LinkerFlag = " "
