set terminal latex
set output "eg5.tex"
set format y "$%g$"
set format x '$%4.1f\pi$'
unset clip points
set title 'This is $\sin(x)$'
set xlabel "This is the $x$ axis"
set ylabel '$\sin(x)$'
unset key
set xtics ('$-\pi$' -pi,\
 '$-\frac{\pi}{2}$' -pi/2,\
 "0" 0,\
 '$\frac{\pi}{2}$' pi/2,\
 '$\pi$' pi)
plot [-pi:pi] [-1:1] sin(x)
