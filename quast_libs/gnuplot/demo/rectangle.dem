# demo for the use of "set object rectangle"
# Updated for version 5
#	show front/back/behind, opaque key box

set object 1 rect from 0,0 to 1,4 fc lt 2 front
set object 2 rect from -1,1 to 0,5 fc rgb "gold"
set object 5 rect from 0, -3 to 2, -2 fc rgb "cyan" fs pattern 1 bo -1

set style fill pattern 2 bo 1

#
# Test clipping and placement in the presence of an inverted axis range
#
set obj 4 rect at -4,0 size 3,1
set label 4 at -2.6,0 "There should be a\nclipped rectangle here" left offset 0,.5

#
# The key box has its own option "opaque" to generate a filled rectangle
#
set key box opaque height 2

#
# Illustrate using character widths to put a box around a label
#
LABEL = "Label in a box"
set obj 10 rect at -3,-4 size char strlen(LABEL), char 1 
set obj 10 fillstyle empty border -1 front
set label 10 at -3,-4 LABEL front center

set obj 9 rect from -4, -4 to -4, -3 fc lt -1

set obj 20 rect from graph 0, graph 0 to graph 1, graph 1 fs solid 0.15 fc rgb "#FFD700" behind

set xrange [5:-5]

plot x, -3+sin(x*5)/x lt 3 lw 3

pause -1 "Hit return to continue"

reset

#
# Requires data file "using.dat" from this directory,
# so change current working directory to this directory before running.
#

set xrange [1:8]
set title "Convex     November 1-7 1989"
set key below
set label "(Weekend)" at 5,25 center

set style rect fc lt -1 fs solid 0.15 noborder

set obj rect from 1, graph 0 to 2, graph 1
set obj rect from 3, graph 0 to 4, graph 1
set obj rect from 5, graph 0 to 6, graph 1
set obj rect from 7, graph 0 to 8, graph 1

plot 'using.dat' using 3:4 title "Logged in" with impulses,\
     'using.dat' using 3:5 t "Load average" with points,\
     'using.dat' using 3:6 t "%CPU used" with lines

pause -1 "Hit return to continue"

reset
