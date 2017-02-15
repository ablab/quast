#set pal gray
#set pal color; set palette rgbformulae 3,11,6
set pm3d map
splot '<awk -f colorpts.awk pts.dat 0.07 0.07'
