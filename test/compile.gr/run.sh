xocc_path="../xocc/xocc.exe"



#### FOLLOW TEST IS DISABLE BOTH VECT AND LSRA ##########################################
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -no-migen -no-lsra -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-vect -prmode -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect -prmode -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-no-cg -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -no-cg -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-cg -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-cg -no-vect -prmode -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-cg -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-cg -no-vect -no-lsra"
if [ $? -ne 0 ]; then echo "EXECUTE PERL FAILED, ERROR CODE = $?" exit 1
fi
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-cg -no-vect -prmode -no-lsra"
if [ $? -ne 0 ]; then  echo "EXECUTE PERL FAILED, ERROR CODE = $?"  exit 1
fi
