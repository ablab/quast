set terminal latex size 7cm, 5cm
set output "eg2.tex"
set format xy "$%g$"
set title 'This is a plot of $y=\sin(x)$'
set xlabel 'This is the $x$ axis'
set ylabel 'This is\\the\\$y$ axis'
plot [0:6.28] [0:1] sin(x)
