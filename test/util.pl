#!/usr/bin/perl -w
use strict;

# these CAN be exported.
our @EXPORT_OK = qw(
    abort 
    computeDirFromFilePath
    computeOutputName
    compileGR 
    findCurrent 
    runSimulator 
    findRecursively 
    findFileRecursively
    generateGR 
    runHostExe
    runArmToolChainToComputeBaseResult
    runPACC 
    runBaseCC 
    runXOCC);
our $g_simulator;
our $g_base_cc;
our $g_xocc;
our $g_pacc;
our $g_host_cc;
our $g_cflags;
our $g_as;
our $g_ld;
our $g_ld_flag;
our $g_is_quit_early; #finish test if error occurred.

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

my $suffix; #used inside this file scope.
my @g_filelist = (); #used inside this file scope.
sub findRecursively {
    my $dir = $_[0];
    $suffix = $_[1];
    @g_filelist = ();
    &find(\&findCore, $dir);
    return @g_filelist;
}

sub findCore {
    push(@g_filelist, $File::Find::name) if ($_ =~ m/\.$suffix$/);
}

my $g_testcase; #used inside this file scope.
sub findFileRecursively {
    my $dir = $_[0];
    $g_testcase = $_[1];
    @g_filelist = ();
    &find(\&findCore2, $dir);
    return @g_filelist;
}

sub findCore2 {
    push(@g_filelist, $File::Find::name) if ($_ =~ m/$g_testcase$/);
}

sub removeLine
{
    my $cmdline;
    my $retval;
    my $xoc_root_path = $_[0];
    my $inputfile = $_[1];
    my $removeline_script_path = $xoc_root_path."test/removeLine.py";
    $cmdline = "$removeline_script_path $inputfile";
    print("\nCMD>>", $cmdline, "\n");
    $retval = system($cmdline);
}

sub runArmToolChainToComputeBaseResult
{
    #Use base-cc to compile, assembly and link.
    my $file = $_[0];
    my $outputfilename = $_[1];
    my $base_result_outputfile = $_[2];
    my $cmdline = "$g_base_cc $file -O0 -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }


    #Run generated binary.
    $cmdline = "qemu-arm -L /usr/arm-linux-gnueabihf/ $outputfilename";
    print("\nCMD>>", $cmdline, " >& ", $base_result_outputfile, "\n");

    open (my $OLDVALUE, '>&', STDOUT); #save stdout to oldvalue
    open (STDOUT, '>>', $base_result_outputfile); #redirect stdout to file
    $retval = system($cmdline);
    open (STDOUT, '>&', $OLDVALUE); #reload stdout from oldvalue

    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $cmdline FAILED!! RES:$retval\n";
        #die($retval);
    }
}


sub runArmExe
{
    my $cmdline;
    my $retval;
    my $binfile = $_[0];
    my $outputfile = $_[1];
    $cmdline = "./$binfile";
    print("\nCMD>>", $cmdline, " >& ", $outputfile, "\n");

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

sub runHostExe
{
    my $cmdline;
    my $retval;
    my $binfile = $_[0];
    my $outputfile = $_[1];
    $cmdline = "./$binfile";
    print("\nCMD>>", $cmdline, " >& ", $outputfile, "\n");

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

sub runSimulator
{
    my $cmdline;
    my $retval;
    my $binfile = $_[0];
    my $outputfile = $_[1];
    $cmdline = "$g_simulator $binfile";
    print("\nCMD>>", $cmdline, " >& ", $outputfile, "\n");

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
        abort($retval);
    }

    if (!-e $grname) { 
        #Not equal
        print "\n$grname does not exist!\n";
        abort();
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
        if ($g_is_quit_early) {
            abort();
        }
    }

    #compile GR to asm
    $cmdline = "$g_xocc $g_cflags $fullpath -readgr $grname -o $asmname";
    print("\nCMD>>", $cmdline, "\n");
    $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }
}


sub runAssembler
{
    my $asmname = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_as $asmname -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_as FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }
}


sub runLinker
{
    my $output = $_[0];
    my $inputasmname = $_[1];
    my $cmdline = "$g_ld $inputasmname -o $output $g_ld_flag";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }
}

#Use base-cc to compile, assembly and link.
sub runBaseCC
{
    my $file = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_base_cc $file -std=c99 -O0 -DSTACK_SIZE=1024 -lm -lc -lm -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }
}

#Use pacc to compile, assembly and link.
sub runPACC 
{
    my $file = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_pacc $file -std=c99 -O0 -DSTACK_SIZE=1024 -lm -lc -lsim  -lm -Tpac.ld -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }
}


#Use pacc to compile, assembly and link.
sub runXOCC
{
    my $cmdline;
    my $src_fullpath = $_[0]; 
    my $is_invoke_assembler = $_[1]; 
    my $is_invoke_linker = $_[2]; 
    my $fname = substr($src_fullpath, rindex($src_fullpath, "/") + 1);
    my $asmname = $src_fullpath.".asm";
    my $outname = computeOutputName($src_fullpath);
    my $objname = $src_fullpath.".o";
    my $is_input_gr = $_[3]; 

    #compile
    unlink($asmname);
    $cmdline = "$g_xocc $g_cflags $src_fullpath -o $asmname";
    if ($is_input_gr) {
        $cmdline = "$cmdline -readgr $src_fullpath";
    } else {
        $cmdline = "$cmdline $src_fullpath";
    }
    print("\nCMD>>", $cmdline, "\n");
    my $retval = system($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        if ($g_is_quit_early) {
            abort($retval);
        }
    }
    if ($is_invoke_assembler) {
        runAssembler($asmname, $objname);
    }
    if ($is_invoke_linker) {
        runLinker($outname, $objname); 
    }
}

sub abort
{
    exit(1);
}

#Compute the output binary file name.
sub computeOutputName
{
    my $fullpath = $_[0];
    return $fullpath.".out";
}

#First param must be file path, not the directory.
#e.g: given /a/b/c.txt, return /a/b/
sub computeDirFromFilePath
{
    my $filepath = $_[0];
    my @segs = split(/\//, $filepath);
    my $seg;
    my $n = 0; #directory level
    my $dir;
    while(defined($seg = $segs[$n])) { $n++; }
    $n--;
    $dir = "";
    my $i = 0; 
    while(defined($seg = $segs[$i])) {
        $dir = "$dir/$seg";
        $i++;
        if ($i >= $n) {
          last;
        }
    }
    return $dir;
}

1;
