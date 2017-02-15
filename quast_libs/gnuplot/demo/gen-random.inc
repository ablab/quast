#
# create random data, used e.g. by 
# random.dem, ellipse.dem, rugplot.dem, and kdensity2d.dem
#
if (!exist("$random")) {
    load "stat.inc"
    nsamp = 3000
    # Generate N random data points.
    set print $random
    do for [i=1:nsamp] {
	print sprintf("%8.5g %8.5g %8.5g", invnorm(rand(0)), invnorm(rand(0)), invnorm(rand(0)))
    }
    unset print
}

