#
#   Use this function to fit sound velocity data as function of
#   sound propagation direction, indicated by its angle against the
#   z-axis of an hexagonal symmetry coordinate system
#
#   Adjustable parameters:
#
#   c33, c11, c13, c44		elastic moduli - materials constants
#   phi0			angle offset
#
# HBB 970522: factored out common factor 1e9 from c?? into the
#  function, to improve the fit convergence. Fit parameters were
#  just too grossly different before.

rho         = 1000.0         # density in kg/m3

phi(x)	    = (x - phi0)/360.0*2.0*pi

main(x)     = c11*sin(phi(x))**2 + c33*cos(phi(x))**2 + c44
mixed(x)    = sqrt( ((c11-c44)*sin(phi(x))**2				\
                    +(c44-c33)*cos(phi(x))**2)**2  \
                   +(2.0*(c13+c44)*sin(phi(x))*cos(phi(x)))**2 ) 

vlong(x)    = sqrt(1.0/2.0/rho*1e9*(main(x) + mixed(x)))
vtrans(x)   = sqrt(1.0/2.0/rho*1e9*(main(x) - mixed(x)))

# When we read the file in, we set y=index, and we use
# y in this (pseudo) 3d fit to choose an x function

f(x,y)	=  y==1  ?  vlong(x)  :  vtrans(x)
