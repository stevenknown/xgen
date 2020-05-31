#!/usr/bin/perl -w

use Cwd;
use File::Find;
use File::Copy;
use File::Path;
use File::Compare;
use strict;
#############################################################
my $g_is_create_base_result = 0;
my $g_is_move_passed_case = 0;
my $g_is_test_gr = 0;
my $g_is_input_gr = 0;
my $g_is_invoke_assembler = 1;
my $g_is_invoke_linker = 1;
my $g_is_invoke_simulator = 1;

#1 if user intend to run testcase recursively, otherwise only
#test current directory.
my $g_is_recur = 0;
my $g_target;

our $g_xocc;
our $g_base_cc;
our $g_is_quit_early = 1; #finish test if error occurred.
my $g_osname = $^O;
my $g_xoc_root_path = "";
my $g_single_testcase = ""; #record the single testcase
our $g_pacc = "pacc";
our $g_host_cc = "gcc";
our $g_as = "pacdsp-elf-as";
our $g_ld = "pacdsp-elf-ld";
our $g_ld_flag = "-lc -lm -lgcc -lsim";
our $g_simulator = "pacdsp-elf-run";
our $g_cflags = "-O1";
computeRelatedPathToXocRootDir();
#computeAbsolutePathToXocRootDir();
require "$g_xoc_root_path/test/util.pl";
#require "d:/X/test/util.pl";
main();
#############################################################
sub main 
{
    $g_target = $ARGV[0];
    if (!$g_target) {
        usage();
        abort();
    }
    parseCmdLine();
    selectTarget();
    printEnvVar();

    # mkpath(["log"]);
    # clean();
    TryCompileAsmLinkRunCompare($g_is_test_gr);

    print "\nTEST FINISH!\n";
}

sub computeAbsolutePathToXocRootDir
{
    my $curdir = getcwd;
    my @segs = split(/\//, $curdir);
    my $seg;

    #Compute absoluately path.
    my $i = 0; #loop index
    $g_xoc_root_path = "/";
    while (defined($seg = $segs[$i])) {
        if ($seg eq "test") {
            last;
        } elsif ($seg ne "") {
            $g_xoc_root_path = $g_xoc_root_path.$seg."/";
        }
        $i++;
        if ($i > 30) {
            #too deep directory
            last;
        }
    }
}

sub computeRelatedPathToXocRootDir
{
    my $curdir = getcwd;
    my @segs = split(/\//, $curdir);
    my $seg;

    #Compute related path.
    my $i = 0; #loop index
    while (defined($seg = $segs[$i])) { $i++; }
    $i--;
    $g_xoc_root_path = "../";
    while (defined($seg = $segs[$i])) {
        if ($seg ne "test") {
            $g_xoc_root_path = "$g_xoc_root_path../";
        } else {
            last;
        }
        $i--;
        if ($i > 30) {
            #too deep directory
            last;
        }
    }
}

sub parseCmdLine
{
    for (my $i = 0; $ARGV[$i]; $i++) {
        if ($ARGV[$i] eq "CreateBaseResult") {
            $g_is_create_base_result = 1;
            next;
        } elsif ($ARGV[$i] eq "CompareNewResult") {
        } elsif ($ARGV[$i] eq "MovePassed") {
            $g_is_move_passed_case = 1;
        } elsif ($ARGV[$i] eq "TestGr") {
            $g_is_test_gr = 1;
        } elsif ($ARGV[$i] eq "InputGr") {
            $g_is_input_gr = 1;
        } elsif ($ARGV[$i] eq "ShowTime") {
            $g_cflags .= " -time";
        } elsif ($ARGV[$i] eq "Recur") {
            $g_is_recur = 1;
        } elsif ($ARGV[$i] eq "NotQuitEarly") {
            $g_is_quit_early = 0;
        } elsif ($ARGV[$i] eq "Case") {
            $i++;
            if (!$ARGV[$i] or ($ARGV[$i] ne "=")) {
                usage();
                abort();
            }
            $i++;
            if (!$ARGV[$i]) {
                usage();
                abort();
            }
            $g_single_testcase = $ARGV[$i];
        } elsif ($ARGV[$i] eq "OnlyCompile") {
            $g_is_invoke_assembler = 0; 
            $g_is_invoke_linker = 0; 
            $g_is_invoke_simulator = 0;
        }
    }
}

sub printEnvVar
{
    print "\n==---- ENVIROMENT VARIABLES ----==\n";
    print "\ng_osname = $g_osname";
    print "\ng_simulator = $g_simulator";
    print "\ng_base_cc = $g_base_cc";
    print "\ng_xocc = $g_xocc";
    print "\ng_pacc = $g_pacc";
    print "\ng_host_cc = $g_host_cc";
    print "\ng_cflags = $g_cflags";
    print "\ng_as = $g_as";
    print "\ng_ld = $g_ld";
    print "\ng_ld_flag = $g_ld_flag";
    print "\ng_is_test_gr = $g_is_test_gr";
    print "\ng_is_input_gr = $g_is_input_gr";
    print "\ng_xoc_root_path = $g_xoc_root_path";
    if ($g_single_testcase ne "") {
        print "\ng_single_testcase = $g_single_testcase";
    }
    print "\n";
}

sub selectTarget
{
    if (!$g_target) {
        usage();
        print "\nNOT SPECIFY A TARGET!\n";
        abort();
    }
    if($g_osname eq 'MSWin32') {
        if ($g_target eq "pac") {
           $g_xocc = "$g_xoc_root_path/src/xocc.prj/Debug/xocc.exe";
           $g_base_cc = "pacc";
           $g_as = "pacdsp-elf-as --horizontaledit";
           $g_ld = "pacdsp-elf-ld -L/home/zhenyu/gj310/install/linux/pacdsp-elf/lib/ -Tpac.ld /home/zhenyu/gj310/install/linux/pacdsp-elf/lib/crt1.o -lc -lsim -lm -lc -lgcc ";
           $g_ld_flag = "-lc -lm -lgcc -lsim";
           $g_simulator = "pacdsp-elf-run";
        } elsif ($g_target eq "arm") {
           $g_base_cc = "arm-linux-gnueabihf-gcc";
           $g_xocc = "$g_xoc_root_path/src/xocc.arm.prj/x64/build/xocc.exe";
           $g_as = "arm-linux-gnueabihf-as";
           $g_ld = "arm-linux-gnueabihf-gcc";
           $g_ld_flag = "";
           $g_simulator = "qemu-arm -L /usr/arm-linux-gnueabihf";
        } elsif ($g_target eq "x86") {
           $g_xocc = "$g_xoc_root_path/src/xocc.x64.prj/debug/xocc.exe";
        }
    } else {
        if ($g_target eq "pac") {
           $g_base_cc = "pacc";
           $g_xocc = "$g_xoc_root_path/src/pac/xocc.exe";
           $g_as = "pacdsp-elf-as --horizontaledit";
           $g_ld = "pacdsp-elf-ld -L/home/zhenyu/gj310/install/linux/pacdsp-elf/lib/ -Tpac.ld /home/zhenyu/gj310/install/linux/pacdsp-elf/lib/crt1.o -lc -lsim -lm -lc -lgcc ";
           $g_ld_flag = "-lc -lm -lgcc -lsim";
           $g_simulator = "pacdsp-elf-run";
        } elsif ($g_target eq "arm") {
           $g_base_cc = "arm-linux-gnueabihf-gcc";
           $g_xocc = "$g_xoc_root_path/src/arm/xocc.exe";
           $g_as = "arm-linux-gnueabihf-as";
           $g_ld = "arm-linux-gnueabihf-gcc";
           $g_ld_flag = "";
           $g_simulator = "qemu-arm -L /usr/arm-linux-gnueabihf";
        } elsif ($g_target eq "x86") {
           $g_xocc = "$g_xoc_root_path/src/x86/xocc.exe";
        }
    }
    #$g_xocc = "/home/zhenyu/x/src.passed_all_execute_test_in_test_exec/xocc/xocc.exe";
    if (!$g_xocc) {
        usage();
        print "\nNOT FIND xocc.exe FOR $g_target!\n";
        abort();
    }
}

sub usage
{
    print "NOTE: You have to make sure the file name of testcase is unqiue.\n";
    print "USAGE: ./run.pl pac|x64|arm [CreateBaseResult] [MovePassed] ",
          "[TestGr] [ShowTime] [OnlyCompile] [Recur] [NotQuitEarly] ",
          "[Case = your_test_file_name]\n";
    print "\nMovePassed:        move passed testcase to 'passed' directory",
          "\n                   NOTE: do not delete testcase in 'passed' directory",
          "\nCreateBaseResult:  generate result if there is no one",
          "\n                   NOTE: deleting the exist one will regenerate base result",
          "\nTestGr:            test GR file",
          "\nInputGr:           inpute file is GR file",
          "\nShowTime:          show compiling time for each compiler pass",
          "\nOnlyCompile:       only compile and assemble testcase",
          "\nRecur:             perform test recursively",
          "\nNotQuitEarly:      perform test always even if there is failure",
          "\nCase = ...:        run single case",
          "\n";
}

sub clean
{
    my $cmdline = "find -name \"*.asm\" | xargs rm -f";
    system($cmdline);
    $cmdline = "rm -rf log";
    system($cmdline);
}


sub TryCompileAsmLinkRunCompare
{   
    #$is_test_gr is true to generate GR and compile GR to asm, then compare the
    #latest output with the base result.
    my $is_test_gr = $_[0];
    #my @f = `find -maxdepth 1 -name "*.c"`;
    my $curdir = getcwd;
    #my @f = findCurrent($curdir, 'c'); 
    my @f; 
    if ($g_single_testcase ne "") {
        @f = findFileRecursively($curdir, $g_single_testcase);
    } elsif ($g_is_recur) {
        if ($g_is_input_gr) {
            @f = findRecursively($curdir, 'gr'); 
        } else {
            @f = findRecursively($curdir, 'c'); 
        }
    } else {
        if ($g_is_input_gr) {
            @f = findCurrent($curdir, 'gr'); 
        } else {
            @f = findCurrent($curdir, 'c'); 
        }
    }
    #my @f = `find -name "*.c"`;

    if ($g_is_create_base_result == 1) {
        #Generate base-output log.
        foreach (@f) {
            chomp;
            my $fullpath = $_; 
            #The baseline result file.
            my $base_output = $fullpath.".base_output.txt";
            if (!-e $base_output) {
                #Generate baseline output result if it does not exist.
                if ($g_target eq "arm") {
                    runArmToolChainToComputeBaseResult(
                        $fullpath, "base.out", $base_output);
                } else {
                    print "\nUNKNOWN TARGET: $g_target!\n";
                    abort();
                }
             }
        }
    }

    foreach (@f) {
        chomp;
        my $fullpath = $_; 
        print "\n-------------------------------------------";
        my $path = substr($fullpath, 0, rindex($fullpath, "/") + 1);
        runXOCC($fullpath, $g_is_invoke_assembler, $g_is_invoke_linker,
                $g_is_input_gr); 
        if ($is_test_gr == 1) {
            generateGRandCompile($fullpath);
        }

        if ($g_is_invoke_simulator) {
            #The new result file.
            my $xocc_output = $fullpath.".xocc_output.txt";
            my $rundir = computeDirFromFilePath($fullpath);

            #my $cmdline = "rm -rf $xocc_output";
            #print("\nCMD>>$cmdline\n");
            #system($cmdline);

            invokeSimulator($fullpath, $curdir, $xocc_output, $rundir);

            #The baseline result file.
            my $base_output = $fullpath.".base_output.txt";

            if ($g_target eq "pac") {
                #Remove the first 2 redundant lines dumped by pac simulator.
                removeLine($g_xoc_root_path, $xocc_output);
            }

            if ($rundir ne $curdir) {
                chdir $curdir;
            }
         
            if (!-e $base_output) { 
                #Baseline output result does not exist.
                print "\nBASE OUTPUT OF $fullpath NOT EXIST!\n";
                abort();
            }
         
            #Compare baseline output and xocc.exe's output.
            print("\nCMD>>compare $base_output $xocc_output\n");
            if (compare($base_output, $xocc_output) == 0) {
                #New result is euqal to baseline result.
                #New result is correct.
            } else {
                #Not equal
                #New result is incorrect!
                print "\nCOMPARE RESULT OF $fullpath FAILED! NOT EQUAL TO BASE RESULT!\n";
                abort();
            }
        }

        if ($g_is_move_passed_case == 1) {
            #Move passed C file to ./passed.
            #NOTE: Do NOT delete testcase file in 'passed' directory.
            my $cmdline;
            $cmdline = "mkdir -p $path/passed/";
            my $retval = system($cmdline);
            if ($retval != 0) {
                print("\nCMD>>", $cmdline, "\n");
                print "\nEXECUTE $cmdline FAILED!! RES:$retval\n";
                if ($g_is_quit_early) {
                    abort();
                }
            }
            print("\nCMD>>move $fullpath, $path/passed\n");
            move($fullpath, $path."/passed/") or
                #die "move failed: $!";
                abort();
        }
    }
}

sub invokeSimulator
{
    my $fullpath = $_[0];
    my $curdir = $_[1];
    my $xocc_output = $_[2];
    my $rundir = $_[3];

    print("\nCMD>>unlink $xocc_output\n");
    unlink($xocc_output);
    my $outname = computeOutputName($fullpath);
    
    #Some testcase need input file to run, the default location of
    #input file is same with testcase.
    if ($rundir ne $curdir) {
        chdir $rundir;
    }
    runSimulator($outname, $xocc_output);
}

sub generateGRandCompile
{
    my $fullpath = $_; 
    my $grname = $fullpath.".gr";
    my $asmname = $grname.".asm";

    generateGR($fullpath);
    compileGR($fullpath);
    if ($g_is_invoke_assembler) {
        runAssembler($g_as, $asmname);
    }
    if ($g_is_invoke_linker) {
        runLinker("xocc.out"); 
    }
}
