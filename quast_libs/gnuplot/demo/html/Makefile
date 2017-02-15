all: html showcase_plots

distclean: clean
clean:
	rm -f *.png *.gnu *.html *.dat epslatex* fit.log soundfit.par *.tmp *.js *.?.svg *.??.svg jsdir

GNUPLOT_LIB = ..

#
# Auto-generation of the html demo set using the png terminal.
#
# Several of the demos need manual intervention:
#	fontfile vector epslatex
#
html: airfoil.html approximate.html arrowstyle.html barchart_art.html binary.html bivariat.html boxplot.html candlesticks.html cerf.html circles.html cities.html complex_trig.html contours.html controls.html dashtypes.html datastrings.html dgrid3d.html discrete.html electron.html ellipse.html enhanced_utf8.html epslatex.html fillbetween.html fillcrvs.html fillstyle.html finance.html fit.html hidden.html hidden2.html histograms.html histograms2.html gantt.html image.html image2.html index.html iterate.html layout.html lines_arrows.html linkedaxes.html margins.html mgr.html multiaxis.html multimsh.html multiplt.html nokey.html parallel.html param.html piecewise.html pm3dcolors.html pm3d.html pm3dgamma.html pointsize.html polar.html poldat.html prob2.html prob.html rainbow.html random.html rectangle.html rgba_lines.html rgbalpha.html rgb_variable.html rugplot.html running_avg.html scatter.html simple.html singulr.html smooth.html spline.html steps.html stringvar.html surface1.html surface2.html transparent.html transparent_solids.html textrotate.html tics.html timedat.html using.html varcolor.html vector.html world.html world2.html animate2.html heatmaps.html stats.html

index.html: index.save
	cp index.save index.html
	cp ../../term/js/*.png .

circles.html: $(GNUPLOT_LIB)/hidden2.dem ./webify.pl
	export DEMOTERM="pngcairo truecolor font 'arial,8' size 500,300"; ./webify.pl circles

cities.html: $(GNUPLOT_LIB)/cities.dem ./webify.pl
	export DEMOTERM="pngcairo font 'Times,8' fontscale 2.5 size 500,500"; ./webify.pl cities

ellipse.html: random.html

hidden2.html: $(GNUPLOT_LIB)/hidden2.dem ./webify.pl
	export DEMOTERM="pngcairo font 'arial,8' size 540,384"; ./webify.pl hidden2

image.html: $(GNUPLOT_LIB)/image.dem ./webify.pl
	export DEMOTERM="pngcairo font 'arial,8' size 540,384"; ./webify.pl image

image2.html: $(GNUPLOT_LIB)/image2.dem ./webify.pl
	export DEMOTERM="pngcairo font 'arial,8' size 540,384"; ./webify.pl image2

rgbalpha.html: $(GNUPLOT_LIB)/rgbalpha.dem ./webify.pl
	export DEMOTERM="pngcairo font 'arial,8' size 540,360"; ./webify.pl rgbalpha

rgb_variable.html: $(GNUPLOT_LIB)/rgb_variable.dem ./webify.pl
	export DEMOTERM="pngcairo font 'arial,8' size 540,360"; ./webify.pl rgb_variable

rugplot.html: random.html

finance.html: $(GNUPLOT_LIB)/finance.dem ./webify.pl
	export DEMOTERM="pngcairo transparent font 'arial,8' size 660,320"; ./webify.pl finance

linkedaxes.html: $(GNUPLOT_LIB)/linkedaxes.dem ./webify.pl
	export DEMOTERM="pngcairo enhanced font 'arial,9' size 610,320"; ./webify.pl linkedaxes 


multiplt.html: $(GNUPLOT_LIB)/multiplt.dem ./webify.pl
	export DEMOTERM="pngcairo transparent font 'arial,8' size 610,480"; ./webify.pl multiplt

transparent.html: $(GNUPLOT_LIB)/transparent.dem ./webify.pl
	export DEMOTERM="pngcairo transparent truecolor font 'arial,8' size 512,280"; ./webify.pl transparent

transparent_solids.html: $(GNUPLOT_LIB)/transparent_solids.dem ./webify.pl
	export DEMOTERM="pngcairo truecolor font 'arial,9' size 512,384"; ./webify.pl transparent_solids

# The png terminal does not support dashed lines, so let's use pngcairo
lines_arrows.html: $(GNUPLOT_LIB)/lines_arrows.dem ./webify.pl
	export DEMOTERM="pngcairo dash dl 2"; ./webify.pl lines_arrows

rgba_lines.html: $(GNUPLOT_LIB)/rgba_lines.dem ./webify.pl
	export DEMOTERM="pngcairo dash dl 2"; ./webify.pl rgba_lines

complex_trig.html: $(GNUPLOT_LIB)/complex_trig.dem ./webify.pl
	export DEMOTERM="pngcairo enhanced font 'times'"; ./webify.pl complex_trig

animate2.html: $(GNUPLOT_LIB)/animate2.dem ./webify.pl
	export DEMOTERM="gif animate transparent noopt delay 10 size 200,200 background rgb 'black'"; ./webify.pl animate2
	mv animate2.html temp.html
	cat temp.html | sed -e 's/1.png/1.gif/g' > animate2.html
	mv animate2.1.png animate2.1.gif
	rm temp.html

showcase_plots: complex_trig.html transparent_solids.html
	cp complex_trig.10.png showcase_plot_1.png
	cp transparent_solids.1.png showcase_plot_2.png

%.html: $(GNUPLOT_LIB)/%.dem ./webify.pl
	./webify.pl $*

