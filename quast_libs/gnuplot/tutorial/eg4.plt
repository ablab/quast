set terminal latex
set output "eg4.tex"
set format y "$%g$"
set format x "$%.2f$"
set title 'This is $\sin(x)$'
set xlabel "This is the $x$ axis"
set ylabel "$\\sin(x)$"
unset key
set xtics -pi, pi/4
plot [-pi:pi] [-1:1] sin(x)
