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
our $g_is_quit_early; #halt test if error occurred.
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
our $g_single_directory; #record the single directory
our $g_succ;
our $g_fail;
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
        abort(); #always quit immediately.
    }
    print "\nTEST FINISH!\n";
    return $g_succ;
}

sub tryCompileAsmLinkRunCompare
{
    #Set $is_test_gr to true to generate GR and compile GR to asm, then compare
    #the latest output with the base result.
    my $is_test_gr = $_[0];
    my $curdir = getcwd;
    if ($g_single_directory ne "") {
        $curdir .= "/".$g_single_directory;
    }
    #Collect files that need to test.
    my @f;
    if ($g_single_testcase ne "") {
        if ($g_is_recur) {
            @f = findFileRecursively($curdir, $g_single_testcase);
        } else {
            @f = findFileCurrent($curdir, $g_single_testcase);
        }
    } elsif ($g_is_recur) {
        @f = findRecursively($curdir, 'gr');
    } else {
        @f = findCurrent($curdir, 'gr');
    }
    if ($g_is_create_base_result == 1) {
        createBaseResultOutputFile($curdir, \@f);
    }
    compileLinkRunForFileList($curdir, $is_test_gr, \@f);
}

sub createBaseResultOutputFile
{
    my $curdir = $_[0];
    my @filelist = @{$_[1]};

    #Generate base-output log.
    foreach (@filelist) {
        chomp;
        my $fullpath = $_;
        #The baseline result file.
        my $base_output = getBaseOutputFilePath($fullpath);
        if (!-e $base_output) {
            #Generate baseline output result if it does not exist.
            if ($g_target eq "arm" || $g_target eq "armhf") {
                runBaseccToolChainToComputeBaseResult($fullpath,
                                                      $base_output, $curdir);
            } else {
                print "\nUNKNOWN TARGET: $g_target!\n";
                abortex();
            }
         }
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
        my $filename = getFileNameFromPath($_);
        my $fullpath = $curdir."/".$filename; 
        print "\n-------------------------------------------";
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
