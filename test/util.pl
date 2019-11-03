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
    computeRelatedPathToXocRootDir
    computeAbsolutePathToXocRootDir
    clean
    runXOCC);
our $g_base_cc;
our $g_xocc;
our $g_pacc = "pacc";
our $g_as = "pacdsp-elf-as";
our $g_ld = "pacdsp-elf-ld";
our $g_simulator = "pacdsp-elf-run";
our $g_cflags = "-O0"; 
our $g_ld_flag;
our $g_is_quit_early = 1; #finish test if error occurred.
our $g_target; #indicate target machine.
our $g_is_create_base_result = 0;
our $g_is_move_passed_case = 0;
our $g_is_test_gr = 0;
our $g_is_input_gr = 0;
our $g_is_invoke_assembler = 1;
our $g_is_invoke_linker = 1;
our $g_is_invoke_simulator = 1;
#1 if user intend to run testcase recursively, otherwise only
#test current directory.
our $g_is_recur = 0;
our $g_osname = $^O;
our $g_xoc_root_path = "";
our $g_single_testcase = ""; #record the single testcase
our $g_override_xocc_path = "";
our $g_is_compare_dump = 0;

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
        #Base compiler might also failed, thus we just compare
        #the result of base compiler even if it is failed.
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
    my $msg = $_[0];
    if ($msg) { 
        print "\n$msg\n";
    }
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

sub prolog
{
    computeRelatedPathToXocRootDir();
    #computeAbsolutePathToXocRootDir();
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
}

sub parseCmdLine
{
    #Skip ARGV[0], it should describe target machine.
    for (my $i = 1; $ARGV[$i]; $i++) {
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
        } else {
            abort("UNSUPPORT COMMAND LINE:'$ARGV[$i]'");
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
          "[TestGr] [ShowTime] [OnlyCompile] [Recur] [NotQuitEarly] [CompareDump] ",
          "[Case = your_test_file_name]  ",
          "[OverrideXoccPath = your_xocc_file_path] ",
          "\n";
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
          "\nCompareDump:       only compile and compare the dump file",
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

#This function infer absolute root path for XOC project directory.
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

#This function infer related root path for XOC project directory.
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

#Extract CFLAG from *.conf and append it to g_cflags.
sub extractAndSetCflag
{
    #Record the configure file.
    my $configure_file_path = $_[0].".conf";
    if (!-e $configure_file_path) { 
        return;
    }
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
    close ($file);
}
1;
