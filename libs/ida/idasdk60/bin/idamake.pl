#! /usr/bin/perl

use Getopt::Std;

getopts("f:D:", \%opt);

$input = $opt{f} ? $opt{f} : "makefile";
$make = $ENV{__BSD__} ? "/usr/local/bin/gmake" : "make";

$fname = "makefile.unx";
if ( not -e $fname )
{
  $fname = "/tmp/makefile.$$";
  open IN, "<$input" or die "Could not open input: $input";
  open OUT, ">$fname" or die "Can't create temp file: $fname";

  while ( <IN> )
  {
    s/^\s*!//;         # remove ! at the beginning
    s#\\(..)#/\1#g; # replace backslashes by slashes
    s/\.mak/.unx/;  # replace .mak by .unx
    s/\r$//;	    # remove ms dos \r
    print OUT $_;
  }

  close IN;
  close OUT;
  $code = system("$make -f $fname $opt{D} @ARGV") != 0;
  system("rm -f $fname");
  exit $code;
}
exec("$make -f $fname @ARGV");

