xocc_path="~/xocc/xocc.exe"

perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -lowest_height -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -lowest_height -prmode -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -nonprdu -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -nonprdu -prdu -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -lowest_height -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -lowest_height -prmode -no-vect -no-lsra"
if [ $? -ne 0 ]; then
  echo "EXECUTE PERL FAILED, ERROR CODE = $?"
  exit 1
fi

### RUN SINGLE CASE ###
#perl run.pl Case = a7_3.c.i Targ = armhf XoccPath = $xocc_path XoccFlag = "-O0 -no-vect -no-lsra" LinkerFlag = "builtin/sqrt.c.i.o builtin/fabs.c.i.o " CompareDumpIfExist CompareResultIfExist

### TEST ALL CASES ###
perl run.pl Targ = armhf XoccPath = $xocc_path XoccFlag = "-O3 -no-vect -no-lsra" CompareDumpIfExist CompareResultIfExist
