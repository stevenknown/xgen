@REM set xocc_path=d:\X\src\xocc.for.pcxac\x64\Debug\xocc.for.pcxac.exe
set xocc_path=d:\X\src\xocc.for.arm\x64\Debug\xocc.for.arm.2.exe

@rem CompareDump
@rem CreateBaseResult
@rem perl run.pl arm XoccPath = %xocc_path% XoccFlag = " -O3 -time " NoAsm NoLink NoRun
@rem perl run.pl arm Case = invert_br.gr XoccPath = %xocc_path% XoccFlag = " " CreateBaseResult CompareDump CompareDumpIfExist
@rem perl run.pl Case = qsort.gr Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)

@rem @rem ### SINGLE CASE ##########################################################
@rem perl run.pl Case = invert_br.gr Targ = arm XoccPath = %xocc_path% NoRun ^
@rem perl run.pl Case = rp_inexact.gr Targ = arm XoccPath = %xocc_path% NoRun ^
@rem    XoccFlag = "-O3 -only-rce -gvn -dce -rp -cp -no-vect -no-lsra -dump-all -dump tmp.log " LinkerFlag = " -lc -lm -lc "

@rem ### GENERATE EXE ##################################################
@rem perl run.pl Targ = arm XoccPath = %xocc_path% NoRun XoccFlag = "-O3 -no-vect -no-lsra" LinkerFlag = "-lc -lm -lc"
@rem @if %errorlevel% neq 0 (@exit /b 2)

@rem @rem #### TEST COMPILE, DISABLE VECT ######################################################################
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-no-cg -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -no-cg -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -no-cg -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)
@rem perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect"
@rem @if %errorlevel% neq 0 (@exit /b 2)

@rem #### TEST COMPILE, DISABLE VECT, LSRA ######################################################################
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-prmode -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -prmode -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -prmode -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -nonprdu -prdu -mdssa -prssa -prmode -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
 
 perl run.pl Targ = arm XoccPath = %xocc_path% CompareDumpIfExist NotQuitEarly NoAsm XoccFlag = "-O3 -lowest_height -nonprdu -prdu -mdssa -prssa -prmode -no-vect -no-lsra"
 @if %errorlevel% neq 0 (@exit /b 2)
