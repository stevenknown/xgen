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
our $g_override_xocc_path;
our $g_is_compare_dump;
require "../util.pl";
prolog();
main();
#############################################################
sub main 
{
    # mkpath(["log"]);
    # clean();
    if ($g_is_compare_dump == 1) {
        TryCompileCompareDump($g_is_test_gr);
    } else {
        TryCompile($g_is_test_gr);
    }

    print "\nTEST FINISH!\n";
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
        extractAndSetCflag($fullpath);
        #Add the dump file path of xocc.exe.
        $g_cflags = $g_cflags." -dump $xocc_dump ";
        runXOCC($fullpath, 0, 0, 0); 
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

sub TryCompile
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
        print "\n-------------------------------------------";
        my $path = substr($fullpath, 0, rindex($fullpath, "/") + 1);

        #Extract CFLAG from *.conf and append it to g_cflags.
        my $org_cflags = $g_cflags;
        extractAndSetCflag($fullpath);
        $g_cflags = $g_cflags." -nocg ";
        runXOCC($fullpath, 0, 0, 0);
        $g_cflags = $org_cflags;
       
        if ($g_is_move_passed_case == 1) {
            #NOTE: Do NOT delete testcase file in 'passed' directory.
            print("\nCMD>>move $fullpath, $path/passed\n");
            move($fullpath, $path."/passed") or abort();
        }
    }
}
