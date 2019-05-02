#!/usr/bin/perl -w
use strict;

# these CAN be exported.
our @EXPORT_OK = qw(
    findCurrent 
    runSimulator 
    generateGR 
    compileGR 
    runPACC 
    runXOCC);
our $g_simulator;
our $g_xocc;
our $g_pacc;
our $g_cflags;
our $g_as;
our $g_ld;

sub findCurrent {
    my $dir = $_[0];
    my $suffix = $_[1];
    my @filelist = ();
    # create a list of all files in
    # the current directory
    opendir(DIR, $dir);
    #@srclist = grep(/\.c$/, readdir(DIR));
    while (my $file = readdir(DIR)) {
        if ($file =~ m/\.$suffix$/) {} #compare the suffix of file.
        else { next; }
        push(@filelist, $dir."/".$file);
    }
    closedir(DIR);
    #print @filelist;
    return @filelist;
}


sub runSimulator
{
    my $cmdline;
    my $retval;
    my $binfile = $_[0];
    my $outputfile = $_[1];
    $cmdline = "$g_simulator $binfile";
    print("\nCMD>>", $cmdline, ">&", $outputfile, "\n");

    open (my $OLDVALUE, '>&', STDOUT); #save stdout to oldvalue
    open (STDOUT, '>>', $outputfile); #redirect stdout to file
    $retval = system($cmdline);
    open (STDOUT, '>&', $OLDVALUE); #reload stdout from oldvalue

    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $cmdline FAILED!! RES:$retval\n";
        #die($retval);
    }
}


#Generate GR from C file.
sub generateGR
{
    my $cmdline;
    my $retval;
    my $fullpath = $_; 
    my $fname = substr($fullpath, rindex($fullpath, "/") + 1);
    my $grname = $fullpath.".gr";
    my $asmname = $grname.".asm";

    #generate GR 
    unlink($grname);
    $cmdline = "$g_xocc $g_cflags $fullpath -dumpgr";
    print("\nCMD>>", $cmdline, "\n");
    $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        die($retval);
    }

    if (!-e $grname) { 
        #Not equal
        print "\n$grname does not exist!\n";
        die "";
    }
    return;
}


#compile GR to asm
sub compileGR
{
    my $cmdline;
    my $retval;
    my $fullpath = $_; 
    my $fname = substr($fullpath, rindex($fullpath, "/") + 1);
    my $grname = $fullpath.".gr";
    my $asmname = $grname.".asm";
    if (!-e $grname) { 
        #Not equal
        print "\n$grname does not exist!\n";
        die "";
    }

    #compile GR to asm
    $cmdline = "$g_xocc $g_cflags $fullpath -readgr $grname -o $asmname";
    print("\nCMD>>", $cmdline, "\n");
    $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        die($retval);
    }
}


sub runAssembler
{
    my $asmname = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_as $asmname -o $outputfilename --horizontaledit";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_as FAILED!! RES:$retval\n";
        die($retval);
    }
}


sub runLinker
{
    my $output = $_[0];
    my $inputasmname = $_[1];
    my $cmdline = "$g_ld $inputasmname -L/home/zhenyu/gj310/install/linux/pacdsp-elf/lib/ -Tpac.ld /home/zhenyu/gj310/install/linux/pacdsp-elf/lib/crt1.o -lc -lsim -lm -lc -lgcc -o $output";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        die($retval);
    }
}


#Use pacc to compile, assembly and link.
sub runPACC 
{
    my $file = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_pacc $file -O0 -DSTACK_SIZE=1024 -lm -lc -lsim  -lm -Tpac.ld -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        die($retval);
    }
}


#Use pacc to compile, assembly and link.
sub runXOCC
{
    my $cmdline;
    my $fullpath = $_[0]; 
    my $is_invoke_assembler = $_[1]; 
    my $is_invoke_linker = $_[2]; 
    my $fname = substr($fullpath, rindex($fullpath, "/") + 1);
    my $asmname = $fullpath.".asm";
   
    #compile
    unlink($asmname);
    $cmdline = "$g_xocc $g_cflags $fullpath -o $asmname";
    print("\nCMD>>", $cmdline, "\n");
    
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        die($retval);
    }
    if ($is_invoke_assembler) {
        runAssembler($asmname, "a.o");
    }
    if ($is_invoke_linker) {
        runLinker("xocc.out", "a.o"); 
    }
}

1;
