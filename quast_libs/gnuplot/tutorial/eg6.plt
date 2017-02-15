set terminal latex  size 5.0,3.0
set output "eg6.tex"
set format y "$%g$"
set format x '$%5.1f\mu$'
set title "This is a title"
set xlabel "This is the $x$ axis"
set ylabel 'This is\\a longer\\version\\ of\\the $y$\\ axis' offset -1
set label "Data" at -5,-5 right
set arrow from -5,-5 to -3.3,-6.7
set key top left
set xtic -10,5,10
plot [-10:10] [-10:10] "eg3.dat" title "Data File"  with linespoints lt 1 pt 7,\
   3*exp(-x*x)+1  title '$3e^{-x^{2}}+1$' with lines lt 4
