#!/usr/bin/perl -w

use Cwd;
use File::Find;
use File::Copy;
use File::Path;
use File::Compare;
use strict;
#############################################################
our $g_is_create_base_result;
our $g_is_move_passed_case;
our $g_is_test_gr;
our $g_is_input_gr;
our $g_is_invoke_assembler;
our $g_is_invoke_linker;
our $g_is_invoke_simulator;
our $g_is_recur;
our $g_target;
our $g_xocc;
our $g_base_cc;
our $g_is_quit_early; #finish test if error occurred.
our $g_osname;
our $g_xoc_root_path;
our $g_single_testcase; #record the single testcase
our $g_pacc;
our $g_as;
our $g_ld;
our $g_ld_flag;
our $g_simulator;
our $g_cflags;
require "../util.pl";
prolog();
main();

#############################################################
sub main 
{
    # mkpath(["log"]);
    # clean();
    TryCompileAsmLinkRunCompare($g_is_test_gr);
    print "\nTEST FINISH!\n";
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

        my $org_cflags = $g_cflags;
        extractAndSetCflag($fullpath);
        runXOCC($fullpath, $g_is_invoke_assembler, $g_is_invoke_linker,
                $g_is_input_gr); 
        $g_cflags = $org_cflags;

        if ($is_test_gr == 1) {
            generateGRandCompile($fullpath);
        }

        if ($g_is_invoke_simulator) {
            #The new result file.
            my $xocc_output = $fullpath.".xocc_output.txt";
            my $rundir = computeDirFromFilePath($fullpath);

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
