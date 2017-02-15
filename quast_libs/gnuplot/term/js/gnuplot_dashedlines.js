/*
 * $Id: gnuplot_dashedlines.js,v 1.3 2012/05/03 20:35:22 sfeam Exp $
 */
/*
 * Ethan Merritt November 2010
 * Add a dashed line method to gnuplot's HTML5 canvas toolset.
 * To start a path use DS(x0,y0) instead of M(x0,y0)
 * To draw line segments use DL(x,y) instead of L(x,y)
 * Finish as usual with ctx.stroke(); ctx.closePath();
 */

gnuplot.solid = [];
gnuplot.dashpattern1 = gnuplot.solid;
gnuplot.dashpattern2 = [ 0.5, 1.0 ];
gnuplot.dashpattern3 = [ .12, .25, .37, .50, .62, .75, .87, 1.0 ];
gnuplot.dashpattern4 = [ 0.50, 0.70, 0.80, 1.0 ];
gnuplot.dashpattern5 = [ 0.30, 0.50, 0.60, 0.75, 0.85, 1.0 ];

gnuplot.dashlength = 200.;	// length of one complete pattern
gnuplot.dashfraction = 0;	// how far in the pattern we got last time
gnuplot.dash_x = 0;
gnuplot.dash_y = 0;

gnuplot.dashtype = function(dt) {
    gnuplot.pattern = dt;
}

gnuplot.dashstart = function(x,y) {
    gnuplot.dash_x = x;
    gnuplot.dash_y = y;
    gnuplot.dashfraction  = 0;
    gnuplot.M(x,y);
}

gnuplot.dashstep = function(x,y) {
    var delx = x - gnuplot.dash_x;
    var dely = y - gnuplot.dash_y;
    var stride = Math.sqrt(delx*delx + dely*dely) / gnuplot.dashlength;
    var this_step; var new_x, new_y;

    if (gnuplot.pattern.length == 0) {gnuplot.L(x,y); return;}

    while (stride > 0) {
        // Find which piece of the pattern we are in
	for (i=0; gnuplot.pattern[i] <= gnuplot.dashfraction; i++);

	this_step = gnuplot.pattern[i] - gnuplot.dashfraction;
	if (stride > this_step) {
            new_x = gnuplot.dash_x + delx*this_step/stride;
            new_y = gnuplot.dash_y + dely*this_step/stride;
	    stride = stride - this_step;
	    gnuplot.dashfraction = gnuplot.pattern[i];
            delx = x - new_x;
            dely = y - new_y;
	} else {
	    new_x = x;
	    new_y = y;
	    gnuplot.dashfraction = gnuplot.dashfraction + stride;
	    stride = 0;
	}
	if (i%2==0) gnuplot.L(new_x,new_y);
	else gnuplot.M(new_x,new_y);

	gnuplot.dash_x = new_x;
	gnuplot.dash_y = new_y;
	if (gnuplot.dashfraction >= 1.0) gnuplot.dashfraction = 0;
    }
}
