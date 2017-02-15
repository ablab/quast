# pm3dConvertToImage.awk
# Written by Petr Mikulik, mikulik@physics.muni.cz
# Code of pm3dImage contributed by Dick Crawford
# Version: 8. 7. 2002
#
# This awk script tries to compress maps in a postscript file created by pm3d
# or gnuplot with pm3d splotting mode. If the input data formed a rectangular
# equidistant map (matrix), then its postscript representation is converted
# into an image operator with 256 levels (of gray or colour). This conversion
# makes the image 20 times smaller.
#
# Usage:
#    gnuplot>set out "|awk -f pm3dConvertToImage.awk >image.ps"
# or
#    your_shell>awk -f pm3dConvertToImage.awk <map.ps >image.ps
#
# Distribution policy: this script belongs to the distribution of pm3d and
# gnuplot programs.
#
# Notes:
#    - no use of run length encoding etc --- you are welcome to contribute
#      an awk implementation
#
# History of changes:
#    - 8.  7. 2002 Petr Mikulik: Don't fail on empty map. Treat properly both
#      cases of scans_in_x.
#    - 4.  7. 2002 Petr Mikulik: Fix for compressing several images in one file.
#    - 3. 10. 2001 Petr Mikulik: Replaced "stroke" by "Stroke" in the "/g"
#      definition - fixes conversion of colour images.
#    - 16. 5. 2000 Petr Mikulik and Dick Crawford: The first version.
#
# License: public domain.


BEGIN {
err = "/dev/stderr"

if (ARGC!=1) {
  print "pm3dConvertToImage.awk --- (c) Petr Mikulik, Brno. Version 8. 7. 2002" >err
  print "Compression of matrix-like pm3d .ps files into 256 levels image. See also" >err
  print "header of this script for more info." >err
  print "Usage:\n\t[stdout | ] awk -f pm3dConvertToImage.awk [<inp_file.ps] >out_file.ps" >err
  print "Example for gnuplot:" >err
  print "\tset out \"|awk -f pm3dConvertToImage.awk >smaller.ps\"" >err
  print "Hint: the region to be converted is between %pm3d_map_begin and %pm3d_map_end" >err
  print "keywords. Rename them to avoid converting specified region." >err
  error = -1
  exit(1)
}

# Setup of main variables.
inside_pm3d_map = 0
pm3d_images = 0

# The following global variables will be used:
error=0
pm3dline=0
scans=0; scan_pts=0; scans_in_x=0
x1=0; y1 = 0; cell_x=0; cell_y=0
x2=0; y2 = 0; x2last=0; y2last=0
}


########################################
# Add definition of pm3dImage to the dictionary
$1=="/BoxFill" && $NF=="def" {
print
print "/Stroke {stroke} def"
print "/pm3dImage {/I exch def gsave		% saves the input array"
print "  /ps 1 string def"
# print "  Color not {/g {setgray} def} if	% avoid stroke in the usual def of /g"
print "  /Stroke {} def			% avoid stroke in the usual def"
print "  I 0 get I 1 get translate I 2 get rotate % translate & rotate"
print "  /XCell I 3 get I 5 get div def	% pixel width"
print "  /YCell I 4 get I 6 get div def	% pixel height"
print "  0 1 I 6 get 1 sub {			% loop over rows"
print "  /Y exch YCell mul def			% save y-coordinate"
print "  0 1 I 5 get 1 sub {			% loop over columns"
print "  XCell mul Y moveto XCell 0 rlineto 0 YCell rlineto"
print "  XCell neg 0 rlineto closepath		% outline pixel"
print "  currentfile ps readhexstring pop	% read hex value"
print "  0 get cvi 255 div g			% convert to [0,1]"
print "  fill } for } for grestore		% fill pixel & close loops"
print "  } def"
next
}

########################################
# Start pm3d map region.

!inside_pm3d_map && $1 == "%pm3d_map_begin" {
inside_pm3d_map = 1
pm3d_images++
# initialize variables for the image description
pm3dline = 0
scans = 1
row[scans] = ""
x2 = -29999.123; y2 = -29999.123
next
}


########################################
# Outside pm3d map region.

!inside_pm3d_map {
if ($1 == "%%Creator:")
    print $0 ", compressed by pm3dConvertToImage.awk"
else if ($1 == "/g" && $2 == "{stroke") {
    # Replace "/g {stroke ...}" by "/g {Stroke ...}" (stroke cannot be used
    # in the pm3dImage region.
    $2 = "{Stroke"
    print
} else print
next
}


########################################
# End of pm3d map region: write all.

$1 == "%pm3d_map_end" {
inside_pm3d_map = 0

if (pm3dline==0) { # empty map
    pm3d_images--;
    next;
}

if (scans_in_x) { grid_y = scan_pts; grid_x = scans; }
  else  { grid_x = scan_pts; grid_y = scans; }

print "Info on pm3d image region number " pm3d_images ": grid " grid_x " x " grid_y >err
print "\tpoints: " pm3dline "  scans: " scans "  start point: " x1","y1 "  end point: " x2","y2 >err

# write image header
print "%pm3d_image_begin"

if (x1 > x2) { x1+=cell_x; x2+=cell_x; } # align offset of the image corner by the cell dimension
if (y1 > y2) { y1+=cell_y; y2+=cell_y; }

#ORIGINAL:
scalex = (grid_x <= 1) ? cell_x : (x2-x1)*(grid_x/(grid_x-1))
scaley = (grid_y <= 1) ? cell_y : (y2-y1)*(grid_y/(grid_y-1))

if (scans_in_x)
    print "[ " x1 " " y1 " 90 " scaley " -" scalex " " grid_y " " grid_x " ] pm3dImage"
else
    print "[ " x1 " " y1 " 0 " scalex " " scaley " " grid_x " " grid_y " ] pm3dImage"

if (scan_pts*scans != pm3dline) {
  print "ERROR: pm3d image is not grid, exiting." >err
  error=1
  exit(8)
}

# write the substituted image stuff
for (i=1; i<=scans; i++) 
  print row[i];

# write the tail of the image environment
print "%pm3d_image_end"

next
}


########################################
# Read in the pm3d map/image data.

{
if (NF!=12 || toupper($2)!="G" || $5!="N") {
	print "ERROR: Wrong (non-pm3d map) data on line " NR ", exiting." >err
	error=1
	exit(8)
}

pm3dline++;

if (pm3dline==1) { # first point of the map
	x1=$3; y1=$4; cell_x=$8;
	x2=x1; y2=y1; cell_y=$9;
} else {
	x2last=x2; y2last=y2;	# remember previous point
	x2=$3; y2=$4;		# remember the current point
}

if (pm3dline==2) { # determine whether data are scans in x or in y
    if (y1==y2) { # y=const, scan in x
	scans_in_x = 0;
	if (x1==x2) { 
	    print "ERROR: First two points are identical?! Exiting." >err
	    error=1
	    exit(5)
	}
    } else { # x=const, scan in y
	if (x1!=x2) {
		print "ERROR: Map is obviously not rectangular, exiting." >err
		error=1
		exit(5)
	}
	scans_in_x = 1;
    }
}

if ( pm3dline>2 && ((!scans_in_x && y2!=y2last) || (scans_in_x && x2!=x2last)) ) {
	if (scans==1) scan_pts = pm3dline-1
	scans++
	row[scans] = ""
}

# now remember the intensity
row[scans] = row[scans] sprintf( "%02X", $1*255 );
next
} # reading map/image data



########################################
# The end.

END {
if (error == 0 && inside_pm3d_map) {
    print "ERROR: Corrupted pm3d block:  \"%pm3d_map_end\"  not found." >err
    error=1
}
if (error==0) {
    if (pm3d_images==0) 
	print "No pm3d image found in the input file." >err
    else
	print "There were " pm3d_images " pm3d image(s) found in the input file." >err
} else if (error>0) {
	    print "An ERROR has been reported. This file is INCORRECT."
	    print "An ERROR has been reported. Output file is INCORRECT." >err
	}
}


# eof pm3dConvertToImage.awk
