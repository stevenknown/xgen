#!/usr/bin/perl -w
use strict;

# These functions are exported.
our @EXPORT_OK = qw(
    abort 
    abortex 
    computeDirFromFilePath
    computeExeName
    computeSourceFileNameByExeName
    compileGR 
    computeRelatedPathToXocRootDir
    computeAbsolutePathToXocRootDir
    compareDumpFile
    clean
    getDumpFilePath
    getBaseResultDumpFilePath
    getOutputFilePath
    getBaseOutputFilePath
    generateGR 
    findCurrent 
    findRecursively 
    findFileRecursively
    is_exist
    moveToPassed
    invokeSimulator
    runSimulator 
    runHostExe
    runArmToolChainToComputeBaseResult
    runBaseCC
    runCPP
    runXOCC
    tryCreateDir
    systemx);

# These variables are exported.
our $g_base_cc = "";
our $g_base_cc_cflags = "";
our $g_xocc = "";
our $g_cpp = "cpp";
our $g_xocc_flag = "";
our $g_cflags = "-O0"; 
our $g_simulator = "";
our $g_as = "";
our $g_ld = "";
our $g_ld_flag = "";
our $g_is_quit_early = 1; #finish test if error occurred.
our $g_target; #indicate target machine.
our $g_is_create_base_result = 0;
our $g_is_move_passed_case = 0;
our $g_is_test_gr = 0;
our $g_is_invoke_assembler = 1;
our $g_is_invoke_linker = 1;
our $g_is_invoke_simulator = 1;
#1 if user intend to run testcase recursively, otherwise only
#test current directory.
our $g_is_recur = 0;
our $g_osname = $^O;
our $g_xoc_root_path = "";
our $g_single_testcase = ""; #record the single testcase
our $g_find_testcase = ""; #record the testcase pattern to be find
our $g_override_xocc_path = "";
our $g_config_file_path = "";
our $g_override_xocc_flag = "";
our $g_is_compare_dump = 0;
our $g_is_compare_result = 0;
our $g_is_nocg = 0;
our $g_is_basedumpfile_must_exist = 0;
our $g_is_baseresultfile_must_exist = 0;
our $g_error_count = 0;

# These functions are imported.
## selectTargetFromConfigFile

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

# Remove one line from start of the given file.
sub removeLine
{
    my $cmdline;
    my $retval;
    my $xoc_root_path = $_[0];
    my $inputfile = $_[1];
    my $num_of_line = $_[2];
    my $removeline_script_path = $xoc_root_path."test/remove_line.py";
    $cmdline = "$removeline_script_path $inputfile $num_of_line";
    print("\nCMD>>", $cmdline, "\n");
    $retval = systemx($cmdline);
}

sub runArmToolChainToComputeBaseResult
{
    #Use base-cc to compile, assembly and link.
    my $file = $_[0];
    my $outputfilename = $_[1];
    my $base_result_outputfile = $_[2];
    my $cmdline = "$g_base_cc $file $g_base_cc_cflags -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        abortex($retval);
    }

    #Run generated binary.
    $cmdline = "qemu-arm -L /usr/arm-linux-gnueabihf/ $outputfilename";
    print("\nCMD>>", $cmdline, " >& ", $base_result_outputfile, "\n");

    open (my $OLDVALUE, '>&', STDOUT); #save stdout to oldvalue
    open (STDOUT, '>>', $base_result_outputfile); #redirect stdout to file
    $retval = systemx($cmdline);
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
    $retval = systemx($cmdline);
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
    $retval = systemx($cmdline);
    open (STDOUT, '>&', $OLDVALUE); #reload stdout from oldvalue

    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $cmdline FAILED!! RES:$retval\n";
        #die($retval);
    }
}

sub runSimulator
{
    my $exefile = $_[0]; #the binary file to execute
    my $outputfile = $_[1]; #redirect stdout to the file
    my $cmdline;
    my $retval;

    if (!-e $exefile) { 
        #Not equal
        print "\n$exefile DOES NOT EXIST!\n";
        abortex();
    }

    $cmdline = "$g_simulator $exefile";
    print("\nCMD>>", $cmdline, "\n");

    open(my $OLDVALUE, '>&', STDOUT); #save stdout to oldvalue
    open(STDOUT, '>>', $outputfile); #redirect stdout to file
    $retval = systemx($cmdline);
    open(STDOUT, '>&', $OLDVALUE); #reload stdout from oldvalue
    if ($retval != 0) {
        #Base compiler might also failed, thus we just compare
        #the result of base compiler even if it is failed.
        abortex("\nEXECUTE $cmdline FAILED!! RES:$retval\n"); #die($retval);
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
    $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        abortex($retval);
    }

    if (!-e $grname) { 
        #Not equal
        print "\n$grname DOES NOT EXIST!\n";
        abortex();
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
        print "\n$grname DOES NOT EXIST!\n";
        abortex();
    }

    #compile GR to asm
    $cmdline = "$g_xocc $g_cflags $grname -o $asmname";
    print("\nCMD>>", $cmdline, "\n");
    $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE XOCC FAILED!! RES:$retval\n";
        abortex($retval);
    }
}

sub runAssembler
{
    my $asmname = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_as $asmname -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_as FAILED!! RES:$retval\n";
        abortex($retval);
    }
}

sub runLinker
{
    my $output = $_[0];
    my $inputasmname = $_[1];
    my $cmdline = "$g_ld $inputasmname -o $output $g_ld_flag";

    print("\nCMD>>", $cmdline, "\n");
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        abortex($retval);
    }
}

#Use base-cc to compile, assembly and link.
sub runBaseCC
{
    my $file = $_[0];
    my $outputfilename = $_[1];
    my $cmdline = "$g_base_cc $file $g_base_cc_cflags -o $outputfilename";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nEXECUTE $g_ld FAILED!! RES:$retval\n";
        abortex($retval);
    }
}

#Use cpp to preprocess C file.
#Output preprocessed file that postfix with *.i.
sub runCPP
{
    my $cmdline;
    my $src_fullpath = $_[0]; 
    my $input_file_name = substr($src_fullpath, rindex($src_fullpath, "/") + 1);
    my $preprocessed_name = $src_fullpath.".i";
    my $exename = computeExeName($src_fullpath);
    my $objname = $src_fullpath.".o";

    #preprcessing
    unlink($preprocessed_name);
    $cmdline = "$g_cpp $src_fullpath -o $preprocessed_name -C";
    print("\nCMD>>", $cmdline, "\n");
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nFAILED! -- EXECUTE CPP FAILED!! RES:$retval\n";
        abortex($retval);
    }
    return $preprocessed_name; 
}

#Use xocc to compile, assembly and link.
sub runXOCC
{
    my $cmdline;
    my $src_fullpath = $_[0]; 
    my $input_file_name = substr($src_fullpath, rindex($src_fullpath, "/") + 1);
    my $is_invoke_assembler = $_[1]; 
    my $is_invoke_linker = $_[2]; 
    my $asmname = $src_fullpath.".asm";
    my $exename = computeExeName($src_fullpath);
    my $objname = $src_fullpath.".o";
    if (!is_exist($g_xocc)) {
        abortex(1);
    }
    if (!is_exist($src_fullpath)) {
        abortex(1);
    }

    #compile
    unlink($asmname);
    $cmdline = "$g_xocc $g_cflags $src_fullpath -o $asmname $g_xocc_flag";
    $cmdline = "$cmdline $src_fullpath";
    print("\nCMD>>$cmdline");
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nFAILED! -- EXECUTE XOCC FAILED!! RES:$retval\n";
        abortex($retval);
    }
    if ($is_invoke_assembler) {
        runAssembler($asmname, $objname);
    }
    if ($is_invoke_linker) {
        runLinker($exename, $objname); 
    }
}

sub abortex
{
    $g_error_count += 1;
    my $msg = $_[0];
    if ($msg) { 
        print "\n$msg\n";
    }
    if (!$g_is_quit_early) {
        return;
    }
    exit(1);
}

sub abort
{
    my $msg = $_[0];
    if ($msg) { 
        print "\n$msg\n";
    }
    exit(1);
}

#Compute the executable binary file name.
sub computeExeName
{
    my $fullpath = $_[0];
    return $fullpath.".out";
}

sub computeSourceFileNameByExeName
{
    my $filepath = $_[0];
    my @segs = split(/\./, $filepath);
    my $seg;
    my $n = 0; #the number of seg
    while (defined($seg = $segs[$n])) { $n++; }

    #Drop the last seg off.
    $n--;
    my $path = "";
    my $i = 0; 
    while (defined($seg = $segs[$i])) {
        if ($i == 0) {
            $path = $seg;
        } else {
            $path = "$path.$seg";
        }
        $i++;
        if ($i >= $n) {
          last;
        }
    }
    return $path;
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
    while (defined($seg = $segs[$n])) { $n++; }

    #Drop the last seg off.
    $n--;
    $dir = "";
    my $i = 0; 
    while (defined($seg = $segs[$i])) {
        $dir = "$dir/$seg";
        $i++;
        if ($i >= $n) {
          last;
        }
    }
    return $dir;
}

sub readConfigFile
{
    if (!-e $g_config_file_path) {
        return;
    }
    require $g_config_file_path;
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
    readConfigFile();
    selectTarget();
    if ($g_is_nocg) {
        $g_cflags = $g_cflags." -nocg ";
    }
    if ($g_override_xocc_path ne "") {
        $g_xocc = $g_override_xocc_path;
    }
    if ($g_override_xocc_flag ne "") {
        $g_xocc_flag = $g_override_xocc_flag;
    }
    printEnvVar();

    #Tools may be in enviroment path.
    #checkExistence();
}

sub checkExistence
{
    my @filelist = ();
    push(@filelist, $g_xocc);
    if ($g_is_invoke_assembler) {
        push(@filelist, $g_as);
    }
    if ($g_is_invoke_linker) {
        push(@filelist, $g_ld);
    }
    if ($g_is_invoke_simulator) {
        push(@filelist, $g_simulator);
    }
    foreach (@filelist) {
       if (!-e $_) { 
           print "\n$_ DOES NOT EXIST!\n";
           if ($g_is_quit_early) {
               abort();
           }
       }
    }
}

sub is_exist
{
    my $filepath = $_[0];
    if (!-e $filepath) { 
        print "\n$filepath DOES NOT EXIST!!\n";
        return 0;
    }
    return 1;
}

sub parseCmdLine
{
    #Skip ARGV[0], it should describe target machine.
    for (my $i = 1; $ARGV[$i]; $i++) {
        if ($ARGV[$i] eq "CreateBaseResult") {
            $g_is_create_base_result = 1;
            next;
        } elsif ($ARGV[$i] eq "CompareNewResult") {
            #nothing to do
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
        } elsif ($ARGV[$i] eq "FindCase") {
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
            $g_find_testcase = $ARGV[$i];
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
        } elsif ($ARGV[$i] eq "NoCG") {
            $g_is_invoke_assembler = 0; 
            $g_is_invoke_linker = 0; 
            $g_is_invoke_simulator = 0;
            $g_is_nocg = 1;
        } elsif ($ARGV[$i] eq "NoAsm") {
            $g_is_invoke_assembler = 0; 
            $g_is_invoke_linker = 0; 
            $g_is_invoke_simulator = 0;
        } elsif ($ARGV[$i] eq "NoLink") {
            $g_is_invoke_linker = 0; 
            $g_is_invoke_simulator = 0;
        } elsif ($ARGV[$i] eq "NoRun") {
            $g_is_invoke_simulator = 0;
        } elsif ($ARGV[$i] eq "CompareDump") {
            $g_is_compare_dump = 1; 
            $g_is_basedumpfile_must_exist = 1;
        } elsif ($ARGV[$i] eq "CompareDumpIfExist") {
            $g_is_compare_dump = 1; 
            $g_is_basedumpfile_must_exist = 0;
        } elsif ($ARGV[$i] eq "CompareResultIfExist") {
            $g_is_compare_result = 1;
            $g_is_baseresultfile_must_exist = 0;
        } elsif ($ARGV[$i] eq "XoccPath") {
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
        } elsif ($ARGV[$i] eq "ConfigFilePath") {
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
            $g_config_file_path = $ARGV[$i];
        } elsif ($ARGV[$i] eq "XoccFlag") {
            $i++;
            if (!$ARGV[$i] or ($ARGV[$i] ne "=")) {
                usage();
                abort();
            }
            $i++;
            if (!$ARGV[$i]) {
                #Note ""(empty string) and undef are both in the case.
                usage();
                abortex();
            }
            $g_override_xocc_flag = $ARGV[$i];
        } elsif ($ARGV[$i] eq "LinkerFlag") {
            $i++;
            if (!$ARGV[$i] or ($ARGV[$i] ne "=")) {
                usage();
                abort();
            }
            $i++;
            if (!$ARGV[$i]) {
                #Note ""(empty string) and undef are both in the case.
                usage();
                abortex();
            }
            $g_ld_flag = $ARGV[$i];
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
    print "\ng_cflags = $g_cflags";
    print "\ng_as = $g_as";
    print "\ng_ld = $g_ld";
    print "\ng_ld_flag = $g_ld_flag";
    print "\ng_is_test_gr = $g_is_test_gr";
    print "\ng_xoc_root_path = $g_xoc_root_path";
    if ($g_find_testcase ne "") {
        print "\ng_find_testcase = $g_find_testcase";
    }
    if ($g_single_testcase ne "") {
        print "\ng_single_testcase = $g_single_testcase";
    }
    if ($g_override_xocc_path ne "") {
        print "\ng_override_xocc_path = $g_override_xocc_path";
    }
    if ($g_override_xocc_flag ne "") {
        print "\ng_override_xocc_flag = $g_override_xocc_flag";
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

    if ($g_osname eq 'MSWin32') {
        if ($g_target eq "arm") {
           $g_xocc = "$g_xoc_root_path/src/xocc.arm.prj/x64/build/xocc.exe";
           $g_base_cc = "arm-linux-gnueabi-gcc";
           $g_as = "arm-linux-gnueabi-as";
           $g_ld = "arm-linux-gnueabi-gcc";
           $g_base_cc_cflags = "-std=c99 -O0";
           $g_simulator = "qemu-arm -L /usr/arm-linux-gnueabihf";
        } elsif ($g_target eq "armhf") {
           $g_xocc = "$g_xoc_root_path/src/xocc.arm.prj/x64/build/xocc.exe";
           $g_base_cc = "arm-linux-gnueabihf-gcc";
           $g_as = "arm-linux-gnueabihf-as";
           $g_ld = "arm-linux-gnueabihf-gcc";
           $g_base_cc_cflags = "-std=c99 -O0";
           $g_simulator = "qemu-arm -L /usr/arm-linux-gnueabihf";
        } elsif ($g_target eq "x86") {
           $g_xocc = "$g_xoc_root_path/src/xocc.x64.prj/debug/xocc.exe";
        } elsif ($g_config_file_path ne "" &&
                 selectTargetFromConfigFile() == 1) {
            ; ## Has already selected target info from config file.
        } else {
            print "\nUNSUPPORT TARGET! PLEASE IMPORT TARGET FROM CONFIG FILE\n";
            abort();
        }
    } else {
        if ($g_target eq "arm") {
            $g_xocc = "$g_xoc_root_path/src/arm/xocc.exe";
            $g_base_cc = "arm-linux-gnueabi-gcc";
            $g_as = "arm-linux-gnueabi-as";
            $g_ld = "arm-linux-gnueabi-gcc";
            $g_simulator = "qemu-arm -L /usr/arm-linux-gnueabihf";
        } elsif ($g_target eq "armhf") {
            $g_xocc = "$g_xoc_root_path/src/arm/xocc.exe";
            $g_base_cc = "arm-linux-gnueabihf-gcc";
            $g_as = "arm-linux-gnueabihf-as";
            $g_ld = "arm-linux-gnueabihf-gcc";
            $g_simulator = "qemu-arm -L /usr/arm-linux-gnueabihf";
        } elsif ($g_target eq "x86") {
            $g_xocc = "$g_xoc_root_path/src/x86/xocc.exe";
        } elsif ($g_config_file_path ne "" &&
                 selectTargetFromConfigFile() == 1) {
            ; ## Has already selected target info from config file.
        } else {
            print "\nUNSUPPORT TARGET! PLEASE IMPORT TARGET FROM CONFIG FILE\n";
            abort();
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
    print "\n==-- NOTE: YOU HAVE TO ENSURE THE FILE NAME OF TESTCASE IS UNQIUE. --==",
          "\n\nUSAGE: ./run.pl x64|arm|armhf [Option List ...]",
          "\n       e.g: ./run.pl arm Case = hello.c XoccFlag = \"-O3\" NotQuitEarly",
          "\n\nOption List can be the following:",
          "\nMovePassed         move passed testcase to 'passed' directory",
          "\n                   NOTE: do not delete testcase in 'passed' directory",
          "\nCreateBaseResult   generate result if there is no one",
          "\n                   NOTE: deleting the exist one will regenerate base result",
          "\nTestGr             generate GR for related C file and test GR file",
          "\nShowTime           show compiling time for each compiler pass",
          "\nRecur              perform test recursively",
          "\nNotQuitEarly       perform test always even if there is failure",
          "\nCase = ...         run single case, e.g: Case = your_test_file_name",
          "\nCompareDump        only compile and compare the dump file",
          "\nCompareDumpIfExist only compile and compare the dump file if the base-dump-file exist",
          "\nXoccPath = ...     refer to xocc.exe path, e.g: XoccPath = your_xocc_file_path",
          "\nXoccFlag = ...     xocc.exe command line option, e.g: XoccFlag = \"-O3 -time\"",
          "\nConfigFilePath = ...",
          "\n                   refer to imported config file path if exist",
          "\nNoCG               do not run Code Generation of xocc",
          "\nNoAsm              do not generate assembly and linking",
          "\nNoLink             do not perform linking",
          "\nNoRun              do not run execuable binary file",
          "\nCompareResultIfExist",
          "\n                   compile result-dump-file of execuable binary file if base-result-dump-file exist",
          "\nNotQuitEarly       do not quit even if any errors occur",
          "\n";
}

sub clean
{
    my $cmdline = "find -name \"*.asm\" | xargs rm -f";
    systemx($cmdline);
    $cmdline = "rm -rf log";
    systemx($cmdline);
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

#The function encapsulates runSimulator and do some preparatory works.
sub invokeSimulator
{
    my $fullpath = $_[0]; #fullpath of source file, NOT the executable file.
    my $curdir = $_[1]; #the directory where current perl invoked.
    my $xocc_output = $_[2]; #redirect stdout to the file
    my $rundir = $_[3]; #the directory where simulator should be run.

    print("\nCMD>>unlink $xocc_output\n");
    unlink($xocc_output);
    my $exefile = computeExeName($fullpath);
    
    #Some testcase need input file to run, the default location of
    #input file is same with testcase.
    if ($rundir ne $curdir) {
        chdir $rundir;
    }
    runSimulator($exefile, $xocc_output);
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

#Extract CFLAG from *.conf if exist and append it to g_cflags.
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
        abortex("FAILED! -- ERROR OPENNING FILE: $!\n");
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

#This function compose and return new file path to output file.
sub getOutputFilePath
{
    my $fullpath = $_[0]; #path to src file.
    my $path = $fullpath.".xocc_output.txt";
    return $path;
}

#This function compose and return new file path to base output file.
sub getBaseOutputFilePath
{
    my $fullpath = $_[0]; #path to src file.
    my $path = $fullpath.".base_output.txt";
    return $path;
}

#This function compose and return new file path to dump file.
sub getDumpFilePath
{
    my $fullpath = $_[0]; #path to src file.
    my $dumpfilepath = $fullpath.".xocc_dump.txt";
    return $dumpfilepath;
}

#This function compose and return new file path to base result dump file.
sub getBaseResultDumpFilePath
{
    my $fullpath = $_[0]; #path to src file.
    my $dumpfilepath = $fullpath.".base_dump.txt";
    return $dumpfilepath;
}

#This function compare the dump file and base file.
sub compareDumpFile
{
    my $fullpath = $_[0]; #path to src file.
    my $dump_file = $_[1]; #new dump file to be compared.
    my $is_basedumpfile_must_exist = $_[2];

    #Compare baseline dump and latest dump.
    #The baseline result file.
    my $base_dump_file = getBaseResultDumpFilePath($fullpath);
    if (!-e $base_dump_file) {
        if ($is_basedumpfile_must_exist) {
            #Baseline dump file does not exist.
            abortex("Base dump file '$base_dump_file' not exist.");
        } else {
            print "\nPASS! NOTE:base dump file '$base_dump_file' not exist.\n";
            return;
        }
    }

    print("\nCMD>>compare dump-file $base_dump_file $dump_file\n");
    if (compare($base_dump_file, $dump_file) == 0) {
        #New result is euqal to baseline result.
        #New result is correct.
        print "\nPASS!\n";
    } else {
        #Not equal
        #New result is incorrect!
        print "\nFAILED! -- COMPARE DUMP OF $fullpath FAILED! NOT EQUAL TO BASE DUMP!\n";
        abortex();
    }
}

sub tryCreateDir
{
    my $path = $_[0]; #path to directory.
    if (-e $path) { return; }
    mkdir($path) or abortex("Can not create directory '$path'");
}

sub moveToPassed
{
    my $fullpath = $_[0]; #path to src file.
    my $path = substr($fullpath, 0, rindex($fullpath, "/") + 1);
    my $passedpath = "$path\/passed\/";

    #Create passed directory.
    tryCreateDir($passedpath);
    #my $cmdline;
    #$cmdline = "mkdir -p $passedpath";
    #my $retval = systemx($cmdline);
    #if ($retval != 0) {
    #    print("\nCMD>>", $cmdline, "\n");
    #    print "\nEXECUTE $cmdline FAILED!! RES:$retval\n";
    #    if ($g_is_quit_early) {
    #        abortex();
    #    }
    #}

    #Move passed C/GR file to directory $fullpath/passed.
    #NOTE: Do NOT delete testcase file in 'passed' directory.
    print("\nCMD>>move $fullpath, $passedpath\n");
    move($fullpath, $passedpath) or abortex();
}

sub systemx
{
    #Perl does not return multiplied exit values. So it returns a 16 bit
    #value, with the exit code in the higher 8 bits. It's often the same,
    #but not always.
    my $cmdline = $_[0];
    my $retval = system($cmdline)/256;
    return $retval;
}

1;
