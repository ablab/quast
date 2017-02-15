set term epslatex color solid
set output 'eg7.tex'
unset border
set dummy u,v
unset key
unset encoding
set parametric
set view 60, 30, 1.2, 1.3
set isosamples 50, 20
set hidden3d offset 1 trianglepattern 3 undefined 1 altdiagonal bentover
set noxtics
set noytics
set noztics
set title "Interlocking Tori - PM3D surface with depth sorting"
set urange [ -3.14159 : 3.14159 ] noreverse nowriteback
set vrange [ -3.14159 : 3.14159 ] noreverse nowriteback
set pm3d depthorder
x=.10; y=.15; dy=.05
set label "left torus:"                           at screen x,y; y=y-dy
set label '$x=\cos u+\frac{1}{2}\cos u \cos v$'   at screen x,y; y=y-dy
set label '$y=\sin u+\frac{1}{2}\sin u \cos v$'   at screen x,y; y=y-dy
set label "$z=\\frac{1}{2}\\sin v$"               at screen x,y 
x=.65; y=.08
set label '\parbox{2.5in}{right torus:\\$x=1+\cos u+\fr\
ac{1}{2}\cos u \cos v$\\$y=\frac{1}{2}\sin v$\\\
$z=\sin u + \frac{1}{2}\sin u \cos v$}' at screen x,y left
set pm3d interpolate 1,1 flush begin noftriangles nohidden3d corners2color mean
splot cos(u)+.5*cos(u)*cos(v),sin(u)+.5*sin(u)*cos(v),.5*sin(v) with pm3d, \
    1+cos(u)+.5*cos(u)*cos(v),.5*sin(v),sin(u)+.5*sin(u)*cos(v) with pm3d

