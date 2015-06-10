#!/usr/bin/perl
# ------------------------------------------------
#   USAGE:    overlay.gm.ps
#   AUTHOR:  William Hayes  -  //95
#   INPUT:   two GeneMark postscript files
#   OUTPUT:  one GeneMark postscript file with the two datasets overlaid
#   Changes:  WSH - (6/3/96) Fixed bugs and added getoptions
#	      WSH - (//95)
# ------------------------------------------------
#   USAGE:    ps.anno descriptionfn psfn "legend" 
#   AUTHOR:  William Hayes  -  8/25/95
#   INPUT:   file with start and stop codons in several formats
#            postscript file which is to be annotated
#   OUTPUT:  annotated ps graph file with special seqs indicated w/ a legend
#   Subroutine:
#          [x y z] 0/1 setdash
#          n setlinewidth
#   Changes:  WSH - (//95)
#              WSH - (//95)
#             JDB 5/2000 - allowed program to work with GM.hmm 2.0 results 
# ------------------------------------------------
# Alex Lomsadze, update for GMS -2014
# combined two scripts into one
#
#  gm   -gk    k - must be !!!
#
# ------------------------------------------------

use warnings;
use strict;
use Getopt::Long;

my $debug    = 0;
my $outfile  = "overlay.ps";
my $lastpage = 0;
my $workdir;
my $int_file = '';
my $legend   = "GeneMark.hmm prediction";

options();

if ($lastpage>0) { $lastpage++; }

my $ps1 = shift(@ARGV);		# first GeneMark postscript file
my $ps2 = shift(@ARGV);		# second GeneMark postscript filex
my $ps3 = shift(@ARGV);		# third GeneMark postscript file

if( !$int_file and !$ps2 ) { usage(); }

if( $ps2 )
{
	open( PS1, "$workdir/$ps1" ) or die "Couldn't open $workdir/$ps1: $!\n"; 
	open( PS2, "$workdir/$ps2" ) or die "Couldn't open $workdir/$ps2: $!\n"; 
	
	if ( $ps3 )
		{ open( PS3, "$workdir/$ps3" ) or die "Couldn't open $workdir/$ps3: $!\n"; }
	else
		{ $ps3 = ""; }
	
	open( PS, ">$workdir/$outfile" ) or die "Couldn't open $workdir/$outfile: $!\n";
	
	#  Pass along lines until we get to actual graph pages
	while( <PS1> )
	{
	    print PS;
	    if (/Page: 2 2/) { last; }
	}
	
	#  Do the same as before but for the second and third file moving to 
	#     first page that shows the graphs
	while( <PS2> )
	{
		last if /%%Page: 2 2/;
	}
	
	if( $ps3 )
	{
		while (<PS3>)
		{
			last if /%%Page: 2 2/;
		}
	}
	
	while( <PS1> )
	{
	    #  Print out file until hit showpage command
	
	    #if !=0 will stop at $lastpage
	    if ($lastpage) {exit if /%%Page: $lastpage/;} 
	
	    #changing certain lines in first file
	    
	    s/newpath/.1 setlinewidth\nnewpath/; 
	    s/0 200 267 printright/0 200 271 printright/; 
	
	    if (/showpage/)
	    {
			# Move second file ptr to right after drawaxiis
			while (<PS2>)
			{
			    last if /drawaxiis/;
			}
			
			if ($ps3 ne "")
			{
				while( <PS3> )
				{
					last if /drawaxiis/;
				}
			}
	
			print PS "\n\n%% Page B $ps2\n";
			
			# Make changes to certain lines
			while (<PS2>)
			{
		    	# add line type and color commands to graphed GeneMark score
		    	s/newpath/.2 setlinewidth [1] 0 setdash 1 0 0 setrgbcolor\nnewpath/;
		    	
		    	# chg color and position of graph data legend
		    	if (/0 200 267 printright/)
		    	{
					print PS "1 0 0 setrgbcolor\n";
		    	}
		    	
		    	if (/showpage/)
		    	{
		    		print PS "0 setgray\n";
		    		last;
		    	}
		    	
		    	print PS;
			}
		
			if( $ps3)
			{
				print PS "\n\n%% Page C $ps3\n";
				# Make changes to certain lines
				
				while( <PS3> )
				{
					# add line type and color commands to graphed GeneMark score
					s/newpath/0.2 setlinewidth [3 1] 0 setdash 0 0 1 setrgbcolor\nnewpath/;
					
					# chg color and position of graph data legend
					if (/0 200 267 printright/)
					{
						s/0 200 267 printright/0 200 263 printright/;
			    		print PS "0 0 1 setrgbcolor\n";
					}
			
					last if /showpage/;
					
					print PS;
				}
			}
	
			#legend at bottom of page
			print PS "0 setgray\n ($ps1) 0 25 8 printleft\n";
			print PS ".1 setlinewidth 25 12 moveto 50 12 lineto stroke\n";
			print PS "1 0 0 setrgbcolor\n($ps2) 0 100 8 printleft\n";
			print PS "0.2 setlinewidth [1] 0 setdash 100 12 moveto 125 12 lineto stroke\n";
			
			if( $ps3 )
			{
				print PS "0 0 1 setrgbcolor\n($ps3) 0 175 8 printleft\n";
				print PS "0.2 setlinewidth [3 1] 0 setdash 175 12 moveto 200 12 lineto stroke\n";
			}
			
			print PS "0 setgray\n\nshowpage\n";
			next;
		}
	    
	    print PS;
	}
	
	close PS;
}

my @lend;
my @rend;
my @strand;

if ( $int_file )
{	
	ParseCoordinates( "$workdir/$int_file" );	
	psanno( "$workdir/$ps1", "$workdir/$outfile", $legend );
}

exit 0;

# ======================  sub ========================
sub psanno
{
	my ( $in, $out, $legend ) = @_;
    
    my $page      =  0;
    my $pagebegin =  0;
    my $axistart  = 30;
    
    my $i;
    my $lnstart;
    my $lnstop;
    my $comp;
    my $htlend;
    my $htrend;
    
    my ( $startpage, $ntperpage, $startnt, $width ) = psgraphattr( $in );
    
    open( IN, $in ) or die "Couldn't open $in: $!\n"; 
    open( OUT, ">", $out ) or die "Couldn't open $out: $!\n";

	while( <IN> )
	{
		if (/Page:\s+(\d+)/)
		{
			$page = $1;
			$pagebegin = ($page-$startpage)*$ntperpage+$startnt;
		}
		
		if( m./home/genmark/runjobs/ghmwork/. )
		{
			s./home/genmark/runjobs/ghmwork/..g ;
		}

		if( /showpage/ && $page>=$startpage )
		{
			printf OUT "[] 0 setdash\n";
            
            for ($i=0;$i<=$#lend ;$i++)
            {
				if ($lend[$i]>$pagebegin+$ntperpage || $rend[$i] < $pagebegin)
					{next;}
				elsif ($lend[$i]-$pagebegin>=0 && $lend[$i]-$pagebegin<=$ntperpage)
				{
					$lnstart=($lend[$i]-$pagebegin)*$width/$ntperpage + $axistart;
                    if ($rend[$i]-$pagebegin>=0 && $rend[$i]-$pagebegin<=$ntperpage)
                    {
                        $lnstop=($rend[$i]-$pagebegin)*$width/$ntperpage + $axistart;
                    }
					else
					{
						$lnstop=$width+$axistart;
					}
				}
				elsif ($lend[$i]-$pagebegin<0 && $rend[$i]-$pagebegin>=0 && $rend[$i]-$pagebegin<=$ntperpage)
				{
					$lnstart=$axistart;
					$lnstop=($rend[$i]-$pagebegin)*$width/$ntperpage + $axistart;
				}
				elsif ($lend[$i]-$pagebegin<0 && $rend[$i]-$pagebegin>$ntperpage)
				{
					$lnstart=$axistart;
					$lnstop=$width + $axistart;
				}
				
				if ($lnstart && $lnstop)
				{
					if ($strand[$i] eq "complement")
						{ $comp=1; }
                    else
                    	{ $comp=0; }

					$htlend = 225 - ($lend[$i]-1+$comp*2)%3*40 - 120*$comp;
					$htrend = 225 - ($rend[$i]-$comp)%3*40 - 120*$comp;

					printf OUT "2 setlinewidth 0 setgray %8.2f %8.2f moveto %8.2f %8.2f lineto stroke\n",$lnstart,$htlend,$lnstop,$htrend;
				}
			}
			
			print OUT "($legend) 0 45 269 printleft\n";
			print OUT "2 setlinewidth 0 setgray 30 270 moveto 40 270 lineto stroke\n";
			print OUT "showpage\n";
		}
		else
		{
			print OUT;
		}
    }
    
    close IN;
    close OUT;
}
# ------------------------------------------------
sub psgraphattr
{ 
	#($startpage,$ntperpage,$startnt,$pgwidth)=&psgraphattr($fn);

	my( $fn ) = @_;
	
	open( IN, $fn ) or die "Couldn't open $fn: $!\n";

	my $pgwidth   = 0;    # width of graph in mm
	my $startpage = 0;    # page on which graph starts

	my $tmppage;
	my $nt1;
	my $nt2;
	my $x1;
	my $x2;
	my $line;
	my $ntperpage;
	my $startnt;
	
	while( <IN> )
	{	
		if (!$startpage && /Page:\s+(\d+)/) {$tmppage=$1;}      # find out which page graph starts
		if (!$startpage && /^drawaxiis/) {$startpage=$tmppage;} # find out which page graph starts
		if (!$pgwidth && /^newpath\s+(\S+)\s+/) {$pgwidth=$1;}  # find out pgwidth of axis in mm
		
		if (/^\s*\((\d+)\)\s+\d+\s+(\S+)\s+\S+\s+printcenter/)
		{	
			$nt1=$1;
			$x1=$2;
            
			while(<IN>)
			{
				last if !/printcenter/;
				$line = $_;
			}
			
			if ($line=~/^\s*\((\d+)\)\s+\d+\s+(\S+)\s+\S+\s+printcenter/) {$nt2=$1; $x2=$2;}
			else {print "Problem getting second set of coordinates\n";}

			$ntperpage = intround($pgwidth/($x2-$x1)*($nt2-$nt1));
			$startnt = $nt1 - intround($ntperpage/$pgwidth*$x1);
			
			last;
		}
	}

	close IN;
    
	return ( $startpage, $ntperpage, $startnt, $pgwidth );
}
# ------------------------------------------------
sub intround
{
	my( $num ) = @_;
	if( $num - int($num) <.5 )
	{
		$num=int($num);
	}
	else
	{
		$num = int($num)+1;
	}
	return $num;
}
# ------------------------------------------------
sub ParseCoordinates
{
	my( $name ) = @_;
	
	open( IN, $name ) or die "Couldn't open $name: $!\n"; 
	
	my $i = -1;
	my $strand;
	
	while( <IN> )
	{
		if( /^\s*\d+\s+(\+|-)\s+\<?(\d+)\s+\>?(\d+)\s+(\d+)/ )
		{
			next if $3 < 30;
			
	        $i++;
	        
	        $strand   = $1;
	        $lend[$i] = $2;
	        $rend[$i] = $3;
	        
	        # JDB
	        
			if( $lend[$i] == 1 )
			{
				my $length = $rend[$i];
				my $x = $length % 3;
				$lend[$i] += $x;
	        }
	
			if( !(($rend[$i] - $lend[$i] + 1)%3 == 0))
			{
				my $length = $rend[$i] - $lend[$i] + 1;
				my $x = $length % 3;
				$rend[$i] -= $x;
			}
			
			# end JDB
	        
	        if( $strand eq "+" )
	        {
	        	$strand[$i] = "direct";
	        }
	        elsif( $strand eq "-" )
	        {
	        	$strand[$i] = "complement";
	        }
	    }
	    elsif( /^(.+)\t(\S+)\t(CDS)\t(\d+)\t(\d+)\t(\S+)\t([\+-])\t(\S+)\t(.+)$/)
	    {
	    	next if( $5 - $4 + 1 < 30 );
			
	        $i++;
	        
	        $strand   = $7;
	        $lend[$i] = $4;
	        $rend[$i] = $5;
	        
	        # JDB
	        
			if( $lend[$i] == 1 )
			{
				my $length = $rend[$i];
				my $x = $length % 3;
				$lend[$i] += $x;
	        }
	
			if( !(($rend[$i] - $lend[$i] + 1)%3 == 0))
			{
				my $length = $rend[$i] - $lend[$i] + 1;
				my $x = $length % 3;
				$rend[$i] -= $x;
			}
			
			# end JDB
	        
	        if( $strand eq "+" )
	        {
	        	$strand[$i] = "direct";
	        }
	        elsif( $strand eq "-" )
	        {
	        	$strand[$i] = "complement";
	        }
	    }
	}
	close IN;
}
# ------------------------------------------------
sub options
{
	if ( $#ARGV == -1 ) { print usage();}
 
	if ( !GetOptions
		(
			'debug'      => \$debug,
			'lastpage:i' => \$lastpage,
			'outfile=s'  => \$outfile,
			'workdir=s'  => \$workdir,
			'int_file=s' => \$int_file,
			'legend=s'   => \$legend
		)
	) { exit 1; }
    
    if( !defined($workdir))
    {
    	$workdir=$ENV{'PWD'};
    }
}
# ------------------------------------------------
sub usage {
    print STDERR <<EndOfUsage;

Usage: $0 [-lastpage num -outfile file] [GeneMark postscript files ...]

    -lastpage num       last page to process
    -outfile file       postscript output filename
    -workdir dirname    directory where all input files can be found
    
    This program is designed to take GeneMark postscript files and
      overlay them on top of each other.  Of course, this will only
      yield useful information if the postscript files are from the
      same exact sequence.  If you have a color postscript viewer or
      printer, the second and third output data show up in red and
      blue respectively.

    Have to have at least two postscript files and can use three 
      postscript files.

    Last page option allows stopping the final postscript file short
      in case you have a huge postscript file and only want a few pages.
EndOfUsage
    exit 1;
}
# ------------------------------------------------
