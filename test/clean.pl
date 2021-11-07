#!/usr/bin/perl -w
use Cwd;
use File::Find;
use File::Copy;
use File::Path;
use File::Compare;
use strict;
#############################################################
our $g_is_quit_early; #finish test if error occurred.
our $g_fail;
our $g_succ;
require "./util.pl";
main();
#############################################################
sub main
{
    $g_is_quit_early = 0;
    my @subdirlist = (
        'compile', 
        'compile.gr', 
        'exec', 
        'exec.gr'
    ); 
    foreach my $subdir (@subdirlist) {
        print "\nENTER DIRECTORY>>$subdir\n";
        chdir $subdir;
        execPerl();
        chdir ".."; #back to parent directory
	}
    return 0;
}

sub execPerl
{
    my $cmdline = "perl clean.pl";
    my $retval = systemx($cmdline);
    if ($retval != 0) {
        print("\nCMD>>", $cmdline, "\n");
        print "\nFAILED! -- EXECUTE CMD FAILED!! RES:$retval\n";
        abortex($retval);
        return $g_fail;
    }
    return $g_succ;
}
