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
my $g_is_invoke_assembler = 1;
my $g_is_invoke_linker = 1;
my $g_is_invoke_simulator = 1;
my $g_is_compare_dump = 0;
my $g_override_xocc_path = "";

#1 if user intend to run testcase recursively, otherwise only
#test current directory.
my $g_is_recur = 0;
my $g_target;

our $g_xocc;
our $g_is_quit_early = 1; #finish test if error occurred.
my $g_osname = $^O;
my $xoc_root_path = "";
my $g_single_testcase = ""; #record the single testcase
our $g_pacc = "pacc";
our $g_as = "pacdsp-elf-as";
our $g_ld = "pacdsp-elf-ld";
our $g_simulator = "pacdsp-elf-run";
our $g_cflags = "-O0"; 
computeXocRootDir();
require "$xoc_root_path/test/util.pl";
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
    if ($g_override_xocc_path ne "") {
        $g_xocc = $g_override_xocc_path;
    }
    printEnvVar();
    if (!-e $g_xocc) { 
        abort("$g_xocc not exist");
    }

    # mkpath(["log"]);
    # clean();
    if ($g_is_compare_dump == 1) {
        TryCompileCompareDump($g_is_test_gr);
    } else {
        TryCompileAsmLinkRunCompare($g_is_test_gr);
    }

    print "\nTEST FINISH!\n";
}

sub computeXocRootDir
{
    my $curdir = getcwd;
    my @segs = split(/\//, $curdir);
    my $seg;
    my $i = 0; #loop index
    while(defined($seg = $segs[$i])) { $i++; }
    $i--;
    $xoc_root_path = "../";
    while(defined($seg = $segs[$i])) {
        if ($seg ne "test") {
            $xoc_root_path = "$xoc_root_path../";
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
        } elsif ($ARGV[$i] eq "CompareDump") {
            $g_is_compare_dump = 1; 
        } elsif ($ARGV[$i] eq "OverrideXoccPath") {
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
            $g_override_xocc_path = $ARGV[$i];
        }
    }
}

sub printEnvVar
{
    print "\n==---- ENVIROMENT VARIABLES ----==\n";
    print "\ng_osname = $g_osname";
    print "\ng_simulator = $g_simulator";
    print "\ng_xocc = $g_xocc";
    print "\ng_pacc = $g_pacc";
    print "\ng_cflags = $g_cflags";
    print "\ng_as = $g_as";
    print "\ng_ld = $g_ld";
    if ($g_single_testcase ne "") {
        print "\ng_single_testcase = $g_single_testcase";
    }
    if ($g_override_xocc_path ne "") {
        print "\ng_override_xocc_path = $g_override_xocc_path";
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
           $g_xocc = "$xoc_root_path/src/xocc.prj/debug/xocc.exe";
        } elsif ($g_target eq "arm") {
           $g_xocc = "$xoc_root_path/src/xocc.arm.prj/x64/Debug/xocc.exe";
        } elsif ($g_target eq "x86") {
           $g_xocc = "$xoc_root_path/src/xocc.x64.prj/debug/xocc.exe";
        }
    } else {
        if ($g_target eq "pac") {
           $g_xocc = "$xoc_root_path/src/pac/xocc.exe";
        } elsif ($g_target eq "arm") {
           $g_xocc = "$xoc_root_path/src/arm/xocc.exe";
        } elsif ($g_target eq "x86") {
           $g_xocc = "$xoc_root_path/src/x86/xocc.exe";
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
    print "USAGE: ./run.pl pac|x64|arm [CreateBaseResult] [MovePassed] ",
          "[TestGr] [ShowTime] [OnlyCompile] [Recur] [NotQuitEarly] ",
          "[CompareDump], [Case = your_test_file_name] ",
          "[OverrideXoccPath = your_xocc_file_path]",
          "\n";
    print "\nMovePassed:        move passed testcase to 'passed' directory",
          "\n                   NOTE: do not delete testcase in 'passed' directory",
          "\nCreateBaseResult:  generate result if there is no one",
          "\n                   NOTE: deleting the exist one will regenerate base result",
          "\nTestGr:            test GR file",
          "\nShowTime:          show compiling time for each compiler pass",
          "\nOnlyCompile:       only compile and assemble testcase",
          "\nCompareDump:       only compile and compare the dump file",
          "\nRecur:             perform test recursively",
          "\nNotQuitEarly:      perform test always even if there is failure",
          "\nCase = ...:        run single case",
          "\nOverrideXoccPath = ...:",
          "\n                   refer xocc path",
          "\n";
}

sub clean
{
    my $cmdline = "find -name \"*.asm\" | xargs rm -f";
    system($cmdline);
    $cmdline = "rm -rf log";
    system($cmdline);
}

sub extractAndSetCflag
{
    #Record the configure file.
    my $configure_file_path = $_[0];
    #Record the dump file path of xocc.exe.
    my $xocc_dump_path = $_[1];

    my $pattern = qr/World/;
    # read file content
    open my $file, '<', $configure_file_path or
        abort("Error openning file: $!\n");
    while (defined(my $line = <$file>)) {
        chomp $line;
        $g_cflags = $g_cflags." ".$line;

        # match the pattern with the content in each line of file
        #if ($line =~ /$pattern/) {
        #   last;
        #}
    }
    $g_cflags = $g_cflags." -dump ".$xocc_dump_path;
    close ($file);
}


sub TryCompileCompareDump
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
        @f = findRecursively($curdir, 'c'); 
    } else {
        @f = findCurrent($curdir, 'c'); 
    }
    #my @f = `find -name "*.c"`;
    foreach (@f) {
        chomp;
        my $fullpath = $_; 

        #Perform comparaion only if configure file exist.
        my $conf_file = $fullpath.".conf";
        if (!-e $conf_file) { 
            #Baseline dump file does not exist.
            next;
        }

        print "\n-------------------------------------------";
        my $path = substr($fullpath, 0, rindex($fullpath, "/") + 1);

        #The new result file.
        my $xocc_dump = $fullpath.".xocc_dump.txt";
        unlink($xocc_dump);

        #Extract CFLAG from *.conf and append it to g_cflags.
        my $org_cflags = $g_cflags;
        extractAndSetCflag($conf_file, $xocc_dump);
        runXOCC($fullpath, 0, 0); 
        $g_cflags = $org_cflags;
       
        #Compare baseline dump and latest dump.
        #The baseline result file.
        my $base_dump = $fullpath.".base_dump.txt";
        if (!-e $base_dump) { 
            #Baseline dump file does not exist.
            abort("Base dump '$base_dump' not exist.");
        }

        print("\nCMD>>compare $base_dump $xocc_dump\n");
        if (compare($base_dump, $xocc_dump) == 0) {
            #New result is euqal to baseline result.
            #New result is correct.
        } else {
            #Not equal
            #New result is incorrect!
            print "\nCOMPARE DUMP OF $fullpath FAILED! NOT EQUAL TO BASE DUMP!\n";
            abort();
        }
        if ($g_is_move_passed_case == 1) {
            #NOTE: Do NOT delete testcase file in 'passed' directory.
            print("\nCMD>>move $fullpath, $path/passed\n");
            move($fullpath, $path."/passed") or abort();
        }
    }
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
        @f = findRecursively($curdir, 'c'); 
    } else {
        @f = findCurrent($curdir, 'c'); 
    }
    #my @f = `find -name "*.c"`;
    foreach (@f) {
        chomp;
        my $fullpath = $_; 
        print "\n-------------------------------------------";
        my $path = substr($fullpath, 0, rindex($fullpath, "/") + 1);
        runXOCC($fullpath, $g_is_invoke_assembler, $g_is_invoke_linker); 
        if ($is_test_gr == 1) {
            generateGRandCompile($fullpath);
        }
        if ($g_is_invoke_simulator) {
            #The baseline result file.
            my $base_output = $fullpath.".base_output.txt";

            #The new result file.
            my $xocc_output = $fullpath.".xocc_output.txt";

            if ($g_is_create_base_result == 1 && !-e $base_output) {
                #Generate baseline output result if it does not exist.
                runPACC($fullpath, "base.out"); 
                runSimulator("base.out", $base_output); 
            }
            unlink($xocc_output);
            my $outname = computeOutputName($fullpath);
            my $rundir = computeDirFromFilePath($fullpath);
            
            #Some testcase need input file to run, the default location of
            #input file is same with testcase.
            if ($rundir ne $curdir) {
                chdir $rundir;
            }
            runSimulator($outname, $xocc_output);
            if ($rundir ne $curdir) {
                chdir $curdir;
            }
         
            if (!-e $base_output) { 
                #Baseline output result does not exist.
                print "\nBASE OUTPUT OF $fullpath NOT EXIST!\n";
                abort();
            }
         
            #Compare baseline output and latest output.
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
            print("\nCMD>>move $fullpath, $path/passed\n");
            move($fullpath, $path."/passed") or abort();
        }
    }
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
