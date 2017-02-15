# Script for transforming points (x y z) into "rectangular surface".
# Can be used to draw colour points in gnuplot with the pm3d enhancement
# Petr Mikulik, 14. 10. 1999

BEGIN {
if (ARGC<4) {
  print "Syntax:  awk -f colorpts.awk file dx dy" >"/dev/stderr"
  print "\nWhere 2*dx, 2*dy defines the point size (enlargement) in x, y" >"/dev/stderr"
  print "Gnuplot usage: set pm3d map; splot '<awk -f colorpts.awk pts.dat 0.01 0.01'" >"/dev/stderr"
  exit
}
dx = ARGV[2]
dy = ARGV[3]
ARGC = 2
}

# skip blank lines
NF==0 { next }

# main
{
x=$1; y=$2
print x-dx "\t" y-dy "\t" $3
print x-dx "\t" y+dy "\t" $3
printf "\n" # blank line (new scan)
print x+dx "\t" y-dy "\t" $3
print x+dx "\t" y+dy "\t" $3
printf "\n\n" # two blank lines (new surface)
}
