#
#   This is a rough approach to fit a model function to the density
#   data of a liquid crystal. The function consists of a linear
#   branch for the high temperature region and of a curved branch with
#   linear asymptote for the low temperatuer branch
#

#   free parameters:
#   m1, m2  slopes of the linear function in the low and high T region
#   Tc	    transition temperature
#   dens_Tc density at the transition temperature
#   g	    factor to scale tanh function

ml	= -0.0001
mh	= -0.0001
dens_Tc = 1.020
Tc	= 45
g	= 1
b	= 0.1


high(x) = mh*(x-Tc) + dens_Tc
lowlin(x)  = ml*(x-Tc) + dens_Tc
curve(x) = b*tanh(g*(Tc-x))

density(x) = x < Tc ? curve(x)+lowlin(x) : high(x)
