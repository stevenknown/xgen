xocc_path="../../src/xocc/xocc.exe"

### OPTIONS
#NotQuitEarly 

#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-no-vect -no-lsra"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -no-vect -no-lsra"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = " -no-vect -no-lsra -no-cg"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -no-vect -no-lsra -no-cg"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -no-vect -no-lsra -no-cg"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
#perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"

### RUN SINGLE CASE ###
#perl run.pl Case = rem.c Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-O3 -no-vect -no-lsra -only-dce -cp -gvn -cfgopt -dce_aggr -dump-all -dump tmp.log " \
#  LinkerFlag = " builtin/_clz32.c.i.o builtin/_clz64.c.i.o \
#    builtin/_divdi3.c.i.o builtin/_moddi3.c.i.o builtin/udivmodsi4.c.i.o \
#    builtin/sqrtf.c.i.o builtin/fabsf.c.i.o " CompareDumpIfExist
#    builtin/sqrt.c.i.o builtin/fabs.c.i.o " CompareDumpIfExist

#perl run.pl Case = test_sqrt.c Targ = armhf XoccPath = $xocc_path \
#XoccFlag = "-O3 -only-rp -rp -gvn -no-vect -no-lsra -dump-all -dump tmp.log -include_region = "sort"" \
#perl run.pl Case = invert_br2.gr Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-O0 -no-vect -no-lsra -dump-all -dump tmp.log" \
#  LinkerFlag = " " CompareDumpIfExist CompareResultIfExist
 

### BUILD ALL CASES ###
#builtin/divmod.c.i.o builtin/_umoddi3.c.i.o builtin/_udivdi3.c.i.o 
#perl run.pl NotQuitEarly Targ = armhf XoccPath = $xocc_path \
#  XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist \
#  LinkerFlag = " builtin/_clz32.c.i.o builtin/_clz64.c.i.o builtin/_divdi3.c.i.o builtin/_moddi3.c.i.o builtin/udivmodsi4.c.i.o builtin/sqrtf.c.i.o builtin/fabsf.c.i.o "
perl run.pl Targ = armhf XoccPath = $xocc_path \
  XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist CompareResultIfExist\
  LinkerFlag = " "
