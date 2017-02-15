#
# Provide a consistent set of four distinguishable
# black line types.
# NB: This does not work with "set term post mono"
#
unset for [i=1:8] linetype i
set linetype 4 dt 1 lw 2 lc rgb "black"
set linetype 3 dt 3 lw 1.5 lc rgb "black"
set linetype 2 dt 2 lw 1.5 lc rgb "black"
set linetype 1 dt solid lw 1 lc rgb "black"
set linetype cycle 4
#
set palette gray
