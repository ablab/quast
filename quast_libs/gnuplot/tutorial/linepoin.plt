# Plots all line types and point types
set term latex
set out "linepoin.tex"
set clip points
set dummy t,y
unset key
set parametric
set samples 14	 # for trange 0:13, we get 14 points: 0, 1, ..., 13
set style function lines
set xtics 1,1,12
set ytics 1,1,6
set title "Up to 6 line types and 12 point types"
set trange [0 : 13]
set xlabel "Points"
set xrange [0 : 13]
set ylabel "Lines"
set yrange [0 : 7]
plot t,1, t,2, t,3, t,4, t,5, t,6, 1,t w p, 2,t w p, 3,t w p, 4,t w p,5,t w p, 6,t w p, 7,t w p, 8,t w p, 9,t w p, 10,t w p, 11,t w p, 12,t w p
