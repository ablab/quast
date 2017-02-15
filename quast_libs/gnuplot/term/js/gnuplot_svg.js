/*
 * $Id: gnuplot_svg.js,v 1.17.2.1 2016/05/16 23:20:22 sfeam Exp $
 */
// Javascript routines for interaction with SVG documents produced by 
// gnuplot's SVG terminal driver.

// Find your root SVG element
var svg = document.querySelector('svg');

// Create an SVGPoint for future math
var pt = svg.createSVGPoint();

// Get point in global SVG space
function cursorPoint(evt){
  pt.x = evt.clientX; pt.y = evt.clientY;
  return pt.matrixTransform(svg.getScreenCTM().inverse());
}

var gnuplot_svg = { };

gnuplot_svg.version = "16 May 2016";

gnuplot_svg.SVGDoc = null;
gnuplot_svg.SVGRoot = null;

gnuplot_svg.Init = function(e)
{
   gnuplot_svg.SVGDoc = e.target.ownerDocument;
   gnuplot_svg.SVGRoot = gnuplot_svg.SVGDoc.documentElement;
   gnuplot_svg.axisdate = new Date();
}

gnuplot_svg.toggleVisibility = function(evt, targetId)
{
   var newTarget = evt.target;
   if (targetId)
      newTarget = gnuplot_svg.SVGDoc.getElementById(targetId);

   var newValue = newTarget.getAttributeNS(null, 'visibility')

   if ('hidden' != newValue)
      newValue = 'hidden';
   else
      newValue = 'visible';

   newTarget.setAttributeNS(null, 'visibility', newValue);

   if (targetId) {
      newTarget = gnuplot_svg.SVGDoc.getElementById(targetId.concat("_keyentry"));
      if (newTarget)
         newTarget.setAttributeNS(null, 'style',
		newValue == 'hidden' ? 'filter:url(#greybox)' : 'none');
   }

   evt.preventDefault();
   evt.stopPropagation();
}

// Mouse tracking echos coordinates to a floating text box

gnuplot_svg.getText = function() {
	return(document.getElementById("coord_text"));
}

gnuplot_svg.updateCoordBox = function(t, evt) {
    /* 
     * Apply screen CTM transformation to the evt screenX and screenY to get 
     * coordinates in SVG coordinate space.  Use scaling parameters stored in
     * the plot document by gnuplot to convert further into plot coordinates.
     * Then position the floating text box using the SVG coordinates.
     */
    var m = document.documentElement.getScreenCTM();
    var p = document.documentElement.createSVGPoint(); 
    var loc = cursorPoint(evt);
    p.x = loc.x;
    p.y = loc.y;
    var label_x, label_y;

    // Allow for scrollbar position (Firefox, others?)
    if (typeof evt.pageX != 'undefined') {
        p.x = evt.pageX; p.y = evt.pageY; 
    }
    t.setAttribute("x", p.x);
    t.setAttribute("y", p.y);
   
    var plotcoord = gnuplot_svg.mouse2plot(p.x,p.y);

    if (gnuplot_svg.plot_timeaxis_x == "DMS" || gnuplot_svg.plot_timeaxis_y == "DMS") {
	if (gnuplot_svg.plot_timeaxis_x == "DMS")
	    label_x = gnuplot_svg.convert_to_DMS(x);
	else
	    label_x = plotcoord.x.toFixed(2);
	if (gnuplot_svg.plot_timeaxis_y == "DMS")
	    label_y = gnuplot_svg.convert_to_DMS(y);
	else
	    label_y = plotcoord.y.toFixed(2);

    } else if (gnuplot_svg.polar_mode) {
	polar = gnuplot_svg.convert_to_polar(plotcoord.x,plotcoord.y);
	label_x = "ang= " + polar.ang.toPrecision(4);
	label_y = "R= " + polar.r.toPrecision(4);

    } else if (gnuplot_svg.plot_timeaxis_x == "Date") {
	gnuplot_svg.axisdate.setTime(1000. * plotcoord.x);
	var year = gnuplot_svg.axisdate.getUTCFullYear();
	var month = gnuplot_svg.axisdate.getUTCMonth();
	var date = gnuplot_svg.axisdate.getUTCDate();
	label_x = (" " + date).slice (-2) + "/"
		+ ("0" + (month+1)).slice (-2) + "/"
		+ year;
	label_y = plotcoord.y.toFixed(2);
    } else if (gnuplot_svg.plot_timeaxis_x == "Time") {
	gnuplot_svg.axisdate.setTime(1000. * plotcoord.x);
	var hour = gnuplot_svg.axisdate.getUTCHours();
	var minute = gnuplot_svg.axisdate.getUTCMinutes();
	var second = gnuplot_svg.axisdate.getUTCSeconds();
	label_x = ("0" + hour).slice (-2) + ":" 
		+ ("0" + minute).slice (-2) + ":"
		+ ("0" + second).slice (-2);
	label_y = plotcoord.y.toFixed(2);
    } else if (gnuplot_svg.plot_timeaxis_x == "DateTime") {
	gnuplot_svg.axisdate.setTime(1000. * plotcoord.x);
	label_x = gnuplot_svg.axisdate.toUTCString();
	label_y = plotcoord.y.toFixed(2);
    } else {
	label_x = plotcoord.x.toFixed(2);
	label_y = plotcoord.y.toFixed(2);
    }

    while (null != t.firstChild) {
    	t.removeChild(t.firstChild);
    }
    var textNode = document.createTextNode(".  "+label_x+" "+label_y);
    t.appendChild(textNode);
}

gnuplot_svg.showCoordBox = function(evt) {
    var t = gnuplot_svg.getText();
    if (null != t) {
    	t.setAttribute("visibility", "visible");
    	gnuplot_svg.updateCoordBox(t, evt);
    }
}

gnuplot_svg.moveCoordBox = function(evt) {
    var t = gnuplot_svg.getText();
    if (null != t)
    	gnuplot_svg.updateCoordBox(t, evt);
}

gnuplot_svg.hideCoordBox = function(evt) {
    var t = gnuplot_svg.getText();
    if (null != t)
    	t.setAttribute("visibility", "hidden");
}

gnuplot_svg.toggleCoordBox = function(evt) {
    var t = gnuplot_svg.getText();
    if (null != t) {
	var state = t.getAttribute('visibility');
	if ('hidden' != state)
	    state = 'hidden';
	else
	    state = 'visible';
	t.setAttribute('visibility', state);
    }
}

gnuplot_svg.toggleGrid = function() {
    if (!gnuplot_svg.SVGDoc.getElementsByClassName) // Old browsers
	return;
    var grid = gnuplot_svg.SVGDoc.getElementsByClassName('gridline');
    for (var i=0; i<grid.length; i++) {
	var state = grid[i].getAttribute('visibility');
	grid[i].setAttribute('visibility', (state == 'hidden') ? 'visible' : 'hidden');
    }
}

gnuplot_svg.showHypertext = function(evt, mouseovertext)
{
    var loc = cursorPoint(evt);
    var anchor_x = loc.x;
    var anchor_y = loc.y;
	
    var hypertextbox = document.getElementById("hypertextbox")
    hypertextbox.setAttributeNS(null,"x",anchor_x+10);
    hypertextbox.setAttributeNS(null,"y",anchor_y+4);
    hypertextbox.setAttributeNS(null,"visibility","visible");

    var hypertext = document.getElementById("hypertext")
    hypertext.setAttributeNS(null,"x",anchor_x+14);
    hypertext.setAttributeNS(null,"y",anchor_y+18);
    hypertext.setAttributeNS(null,"visibility","visible");

    var lines = mouseovertext.split('\n');
    var height = 2+16*lines.length;
    hypertextbox.setAttributeNS(null,"height",height);
    var length = hypertext.getComputedTextLength();
    hypertextbox.setAttributeNS(null,"width",length+8);

    // bounce off frame bottom
    if (anchor_y > gnuplot_svg.plot_ybot + 16 - height) {
	anchor_y -= height;
	hypertextbox.setAttributeNS(null,"y",anchor_y+4);
	hypertext.setAttributeNS(null,"y",anchor_y+18);
    }

    while (null != hypertext.firstChild) {
        hypertext.removeChild(hypertext.firstChild);
    }

    var textNode = document.createTextNode(lines[0]);

    if (lines.length <= 1) {
	hypertext.appendChild(textNode);
    } else {
	xmlns="http://www.w3.org/2000/svg";
	var tspan_element = document.createElementNS(xmlns, "tspan");
	tspan_element.appendChild(textNode);
	hypertext.appendChild(tspan_element);
	length = tspan_element.getComputedTextLength();
	var ll = length;

	for (var l=1; l<lines.length; l++) {
	    var tspan_element = document.createElementNS(xmlns, "tspan");
	    tspan_element.setAttributeNS(null,"dy", 16);
	    textNode = document.createTextNode(lines[l]);
	    tspan_element.appendChild(textNode);
	    hypertext.appendChild(tspan_element);

	    ll = tspan_element.getComputedTextLength();
	    if (length < ll) length = ll;
	}
	hypertextbox.setAttributeNS(null,"width",length+8);
    }

    // bounce off right edge
    if (anchor_x > gnuplot_svg.plot_xmax + 14 - length) {
	anchor_x -= length;
	hypertextbox.setAttributeNS(null,"x",anchor_x+10);
	hypertext.setAttributeNS(null,"x",anchor_x+14);
    }

    // left-justify multiline text
    var tspan_element = hypertext.firstChild;
    while (tspan_element) {
	tspan_element.setAttributeNS(null,"x",anchor_x+14);
	tspan_element = tspan_element.nextElementSibling;
    }

}

gnuplot_svg.hideHypertext = function ()
{
    var hypertextbox = document.getElementById("hypertextbox")
    var hypertext = document.getElementById("hypertext")
    hypertextbox.setAttributeNS(null,"visibility","hidden");
    hypertext.setAttributeNS(null,"visibility","hidden");
}

// Convert from svg panel mouse coordinates to the coordinate
// system of the gnuplot figure
gnuplot_svg.mouse2plot = function(mousex,mousey) {
    var plotcoord = new Object;
    var plotx = mousex - gnuplot_svg.plot_xmin;
    var ploty = mousey - gnuplot_svg.plot_ybot;
    var x,y;

    if (gnuplot_svg.plot_logaxis_x != 0) {
	x = Math.log(gnuplot_svg.plot_axis_xmax)
	  - Math.log(gnuplot_svg.plot_axis_xmin);
	x = x * (plotx / (gnuplot_svg.plot_xmax - gnuplot_svg.plot_xmin))
	  + Math.log(gnuplot_svg.plot_axis_xmin);
	x = Math.exp(x);
    } else {
	x = gnuplot_svg.plot_axis_xmin + (plotx / (gnuplot_svg.plot_xmax-gnuplot_svg.plot_xmin)) * (gnuplot_svg.plot_axis_xmax - gnuplot_svg.plot_axis_xmin);
    }

    if (gnuplot_svg.plot_logaxis_y != 0) {
	y = Math.log(gnuplot_svg.plot_axis_ymax)
	  - Math.log(gnuplot_svg.plot_axis_ymin);
	y = y * (ploty / (gnuplot_svg.plot_ytop - gnuplot_svg.plot_ybot))
	  + Math.log(gnuplot_svg.plot_axis_ymin);
	y = Math.exp(y);
    } else {
	y = gnuplot_svg.plot_axis_ymin + (ploty / (gnuplot_svg.plot_ytop-gnuplot_svg.plot_ybot)) * (gnuplot_svg.plot_axis_ymax - gnuplot_svg.plot_axis_ymin);
    }

    plotcoord.x = x;
    plotcoord.y = y;
    return plotcoord;
}

gnuplot_svg.convert_to_polar = function (x,y)
{
    polar = new Object;
    var phi, r;
    phi = Math.atan2(y,x);
    if (gnuplot_svg.plot_logaxis_r) 
        r = Math.exp( (x/Math.cos(phi) + Math.log(gnuplot_svg.plot_axis_rmin)/Math.LN10) * Math.LN10);
    else
        r = x/Math.cos(phi) + gnuplot_svg.plot_axis_rmin;
    polar.ang = phi * 180./Math.PI;
    polar.r = r;
    return polar;
}

gnuplot_svg.convert_to_DMS = function (x)
{
    var dms = {d:0, m:0, s:0};
    var deg = Math.abs(x);
    dms.d = Math.floor(deg);
    dms.m = Math.floor((deg - dms.d) * 60.);
    dms.s = Math.floor((deg - dms.d) * 3600. - dms.m * 60.);
    fmt = ((x<0)?"-":" ")
        + dms.d.toFixed(0) + "°"
	+ dms.m.toFixed(0) + "\""
	+ dms.s.toFixed(0) + "'";
    return fmt;
}
