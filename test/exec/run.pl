#!/usr/bin/perl -w

use Cwd;
use File::Find;
use File::Copy;
use File::Path;
use File::Compare;
use strict;
#############################################################
# These are imported variables.
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
our $g_find_testcase; #record the single testcase
our $g_is_compare_result;
our $g_is_baseresultfile_must_exist;
our $g_as;
our $g_ld;
our $g_ld_flag;
our $g_simulator;
our $g_cflags;
our $g_error_count;
require "../util.pl";
prolog();
main();

#############################################################
sub main 
{
    # mkpath(["log"]);
    # clean();
    tryCompileAsmLinkRunCompare($g_is_test_gr);
    if ($g_error_count != 0) {
        print "\nThere are $g_error_count error occurred!\n";
    }
    print "\nTEST FINISH!\n";
}

sub createBaseResultOutputFile
{
    my @filelist = @_; 

    #Generate base-output log.
    foreach (@filelist) {
        chomp;
        my $fullpath = $_; 
        #The baseline result file.
        my $base_output = getBaseOutputFilePath($fullpath);
        if (!-e $base_output) {
            #Generate baseline output result if it does not exist.
            if ($g_target eq "arm" || $g_target eq "armhf") {
                runArmToolChainToComputeBaseResult($fullpath, "base.out",
                                                   $base_output);
            } else {
                print "\nUNKNOWN TARGET: $g_target!\n";
                abortex();
            }
         }
    }
}

sub run
{
    my $curdir = $_[0];
    my $fullpath = $_[1];

    #The new result file.
    my $xocc_output = getOutputFilePath($fullpath);
    my $rundir = computeDirFromFilePath($fullpath);

    invokeSimulator($fullpath, $curdir, $xocc_output, $rundir);

    if (!$g_is_compare_result) {
        return;
    }

    #The baseline result file.
    my $base_output = getBaseOutputFilePath($fullpath);

    if ($g_target eq "apc") {
        #Remove the first 2 redundant lines dumped by apc simulator.
        removeLine($g_xoc_root_path, $xocc_output, 2);
    }

    if ($rundir ne $curdir) {
        chdir $curdir;
    }

    if (!-e $base_output) {
        if ($g_is_baseresultfile_must_exist) {
            #Baseline output result does not exist.
            print "\nBASE OUTPUT OF $fullpath NOT EXIST!\n";
            abortex();
        }
        return;
    }

    #Compare baseline output and xocc.exe's output.
    print("\nCMD>>compare $base_output $xocc_output\n");
    if (compare($base_output, $xocc_output) == 0) {
        #New result is euqal to baseline result.
        #New result is correct.
        print "\nPASS!\n";
    } else {
        #Not equal
        #New result is incorrect!
        print "\nCOMPARE RESULT OF $fullpath FAILED! NOT EQUAL TO BASE RESULT!\n";
        abortex();
    }
}


sub compileLinkRunForFileList
{
    my $curdir = $_[0];
    my $is_test_gr = $_[1];
    
    #Method1 to ref array argument: 
    #my $filelist = $_[2];
    #my $firstfile = $filelist->[0];
    #foreach (@$filelist) {
    
    #Method2 to ref array argument: 
    my @filelist = @{$_[2]};
    my $firstfile = $filelist[0];
    foreach (@filelist) {
        chomp;
        my $fullpath = $_; 
        print "\n-------------------------------------------";
        #Extract path prefix from 'fullpath'.
        #e.g: $fullpath is /a/b/c.cpp, then $path is /a/b/
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
            run($curdir, $fullpath); 
        }

        if ($g_is_move_passed_case == 1) {
            moveToPassed($fullpath);
        }
    }
}

sub tryCompileAsmLinkRunCompare
{
    #$is_test_gr is true to generate GR and compile GR to asm, then compare the
    #latest output with the base result.
    my $is_test_gr = $_[0];
    #my @f = `find -maxdepth 1 -name "*.c"`;
    my $curdir = getcwd;
    #my @f = findCurrent($curdir, 'c'); 
    my @f; 
    if ($g_single_testcase ne "") {
        @f = $curdir."/".$g_single_testcase;
    } elsif ($g_find_testcase ne "") {
        @f = findFileRecursively($curdir, $g_find_testcase);
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
        createBaseResultOutputFile(@f);
    }

    compileLinkRunForFileList($curdir, $is_test_gr, \@f);
}
