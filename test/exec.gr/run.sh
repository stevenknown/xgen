xocc_path="../../src/xocc/xocc.exe"


perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = ""
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3"
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height"
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-no-cg"
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -no-cg"
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -no-cg"
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa"
perl run.pl Targ = arm XoccPath = $xocc_path CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa"
