@rem SET XOCC.EXE PATH HERE.
set xocc_path=../xocc/xocc.exe

perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -no-migen -no-lsra -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-vect -prmode -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect -prmode -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-no-cg -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -no-cg -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-cg -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -no-cg -no-vect -prmode -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-cg -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-cg -no-vect -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
perl run.pl Targ = arm Dir = nocg XoccPath = %xocc_path% NoAsm NoLink NoRun XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-cg -no-vect -prmode -no-lsra"
@if %errorlevel% neq 0 (@exit /b 2)
@exit /b 0
