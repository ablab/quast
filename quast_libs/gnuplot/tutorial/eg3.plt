set terminal latex
set output "eg3.tex"
set format xy "$%g$"
set title "This is another plot"
set xlabel "$x$ axis"
set ylabel "$y$ axis"
set key at 15,-10
plot x with lines, "eg3.dat" with linespoints
