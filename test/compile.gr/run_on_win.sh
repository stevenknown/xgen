xocc_path="../xocc/xocc.exe"

perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = ""
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3"
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height"
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa"
perl run.pl Targ = arm XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa"
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-no-cg"
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -no-cg"
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-cg"
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-cg"
perl run.pl Targ = arm Dir = nocg XoccPath = $xocc_path NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-cg"

