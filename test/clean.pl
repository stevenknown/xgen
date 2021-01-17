#!/usr/bin/perl -w
use Cwd;
use File::Find;
use File::Copy;
use File::Path;
use Data::Dump qw(dump);
###########################################
my $curdir = getcwd;
removeDesignatedFile();
removeDirectory();
exit();
###########################################

## Remove designated file.
my @srclist = ();
sub removeDesignatedFile
{
    my $project_dir = $curdir;
    sub wanted
    {
        push(@srclist, $File::Find::name) if ($_ =~ m/.*\.(xocc_output.txt|xocc_dump.txt|o|i|map|vcg|js|d.ts|swp|swo|asm|out)$/);
    }
    &find(\&wanted, $project_dir);
    foreach (@srclist){
        chomp;
        print "=========\nFile: ", $_, "\n";
        unlink $_;
    }
}

## Remove directory.
sub removeDirectory
{
    my @f2 = (
    	"$curdir\\ipch",
    	"$curdir\\Debug",
    	"$curdir\\Release",
    	"$curdir\\*.suo",
    	"$curdir\\.vs",
    	"$curdir\\tmp",
    );
    foreach (@f2) {
    	chomp;
    	print "\nrmtree:$_";
    	my $retval = rmtree($_);
        if ($retval != 0) {
            print("\nFailed:retval=$retval");
    	}
    }
}
