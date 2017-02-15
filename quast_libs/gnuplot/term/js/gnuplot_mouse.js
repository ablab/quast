/*
 * $Id: gnuplot_mouse.js,v 1.22.2.2 2014/09/26 20:42:56 sfeam Exp $
 */
    gnuplot.mouse_version = " 26 September 2014";

// Mousing code for use with gnuplot's 'canvas' terminal driver.
// The functions defined here assume that the javascript plot produced by
// gnuplot initializes the plot boundary and scaling parameters.

    gnuplot.mousex = 0;
    gnuplot.mousey = 0;
    gnuplot.plotx = 0;
    gnuplot.ploty = 0;
    gnuplot.scaled_x = 0;
    gnuplot.scaled_y = 0;

// These will be initialized by the gnuplot canvas-drawing function
    gnuplot.plot_xmin = 0;
    gnuplot.plot_xmax = 0;
    gnuplot.plot_ybot = 0;
    gnuplot.plot_ytop = 0;
    gnuplot.plot_width  = 0
    gnuplot.plot_height = 0
    gnuplot.plot_term_ymax = 0;
    gnuplot.plot_axis_xmin = 0;
    gnuplot.plot_axis_xmax = 0;
    gnuplot.plot_axis_width  = 0;
    gnuplot.plot_axis_height = 0;
    gnuplot.plot_axis_ymin = 0;
    gnuplot.plot_axis_ymax = 0;
    gnuplot.plot_axis_x2min = "none";
    gnuplot.plot_axis_y2min = "none";
    gnuplot.plot_logaxis_x = 0;
    gnuplot.plot_logaxis_y = 0;
    gnuplot.grid_lines = true;
    gnuplot.zoom_text = false;
    gnuplot.display_is_uptodate = false;

// These are the equivalent parameters while zooming
    gnuplot.zoom_axis_xmin = 0;
    gnuplot.zoom_axis_xmax = 0;
    gnuplot.zoom_axis_ymin = 0;
    gnuplot.zoom_axis_ymax = 0;
    gnuplot.zoom_axis_x2min = 0;
    gnuplot.zoom_axis_x2max = 0;
    gnuplot.zoom_axis_y2min = 0;
    gnuplot.zoom_axis_y2max = 0;
    gnuplot.zoom_axis_width = 0;
    gnuplot.zoom_axis_height = 0;
    gnuplot.zoom_temp_xmin = 0;
    gnuplot.zoom_temp_ymin = 0;
    gnuplot.zoom_temp_x2min = 0;
    gnuplot.zoom_temp_y2min = 0;
    gnuplot.zoom_in_progress = false;

    gnuplot.full_canvas_image = null;
    gnuplot.axisdate = new Date();

gnuplot.init = function ()
{
  if (document.getElementById("gnuplot_canvas"))
      document.getElementById("gnuplot_canvas").onmousemove = gnuplot.mouse_update;
  if (document.getElementById("gnuplot_canvas"))
      document.getElementById("gnuplot_canvas").onmouseup = gnuplot.zoom_in;
  if (document.getElementById("gnuplot_canvas"))
      document.getElementById("gnuplot_canvas").onmousedown = gnuplot.saveclick;
  if (document.getElementById("gnuplot_canvas"))
      document.getElementById("gnuplot_canvas").onkeydown = gnuplot.do_hotkey;
  if (document.getElementById("gnuplot_grid_icon"))
      document.getElementById("gnuplot_grid_icon").onmouseup = gnuplot.toggle_grid;
  if (document.getElementById("gnuplot_textzoom_icon"))
      document.getElementById("gnuplot_textzoom_icon").onmouseup = gnuplot.toggle_zoom_text;
  if (document.getElementById("gnuplot_rezoom_icon"))
      document.getElementById("gnuplot_rezoom_icon").onmouseup = gnuplot.rezoom;
  if (document.getElementById("gnuplot_unzoom_icon"))
      document.getElementById("gnuplot_unzoom_icon").onmouseup = gnuplot.unzoom;
  gnuplot.mouse_update();
}

gnuplot.getMouseCoordsWithinTarget = function(event)
{
	var coords = { x: 0, y: 0};

	if(!event) // then we're in a non-DOM (probably IE) browser
	{
		event = window.event;
		if (event) {
			coords.x = event.offsetX;
			coords.y = event.offsetY;
		}
	}
	else		// we assume DOM modeled javascript
	{
		var Element = event.target ;
		var CalculatedTotalOffsetLeft = 0;
		var CalculatedTotalOffsetTop = 0 ;

		while (Element.offsetParent)
 		{
 			CalculatedTotalOffsetLeft += Element.offsetLeft ;     
			CalculatedTotalOffsetTop += Element.offsetTop ;
 			Element = Element.offsetParent ;
 		}

		coords.x = event.pageX - CalculatedTotalOffsetLeft ;
		coords.y = event.pageY - CalculatedTotalOffsetTop ;
	}

	gnuplot.mousex = coords.x;
	gnuplot.mousey = coords.y;
}


gnuplot.mouse_update = function(e)
{
  gnuplot.getMouseCoordsWithinTarget(e);

  gnuplot.plotx = gnuplot.mousex - gnuplot.plot_xmin;
  gnuplot.ploty = -(gnuplot.mousey - gnuplot.plot_ybot);

  // Limit tracking to the interior of the plot
  if (gnuplot.plotx < 0 || gnuplot.ploty < 0) return;
  if (gnuplot.mousex > gnuplot.plot_xmax || gnuplot.mousey < gnuplot.plot_ytop) return;

  var axis_xmin = (gnuplot.zoomed) ? gnuplot.zoom_axis_xmin : gnuplot.plot_axis_xmin;
  var axis_xmax = (gnuplot.zoomed) ? gnuplot.zoom_axis_xmax : gnuplot.plot_axis_xmax;
  var axis_ymin = (gnuplot.zoomed) ? gnuplot.zoom_axis_ymin : gnuplot.plot_axis_ymin;
  var axis_ymax = (gnuplot.zoomed) ? gnuplot.zoom_axis_ymax : gnuplot.plot_axis_ymax;

    if (gnuplot.plot_logaxis_x != 0) {
	x = Math.log(axis_xmax) - Math.log(axis_xmin);
	x = x * (gnuplot.plotx / (gnuplot.plot_xmax-gnuplot.plot_xmin)) + Math.log(axis_xmin);
	x = Math.exp(x);
    } else {
	x =  axis_xmin + (gnuplot.plotx / (gnuplot.plot_xmax-gnuplot.plot_xmin)) * (axis_xmax - axis_xmin);
    }

    if (gnuplot.plot_logaxis_y != 0) {
	y = Math.log(axis_ymax) - Math.log(axis_ymin);
	y = y * (-gnuplot.ploty / (gnuplot.plot_ytop-gnuplot.plot_ybot)) + Math.log(axis_ymin);
	y = Math.exp(y);
    } else {
	y =  axis_ymin - (gnuplot.ploty / (gnuplot.plot_ytop-gnuplot.plot_ybot)) * (axis_ymax - axis_ymin);
    }

    if (gnuplot.plot_axis_x2min != "none") {
	gnuplot.axis_x2min = (gnuplot.zoomed) ? gnuplot.zoom_axis_x2min : gnuplot.plot_axis_x2min;
	gnuplot.axis_x2max = (gnuplot.zoomed) ? gnuplot.zoom_axis_x2max : gnuplot.plot_axis_x2max;
	if (gnuplot.x2_mapping != undefined)
	    x2 = gnuplot.x2_mapping(x);
	else
	    x2 =  gnuplot.axis_x2min + (gnuplot.plotx / (gnuplot.plot_xmax-gnuplot.plot_xmin)) * (gnuplot.axis_x2max - gnuplot.axis_x2min);
	if (document.getElementById(gnuplot.active_plot_name + "_x2"))
	    document.getElementById(gnuplot.active_plot_name + "_x2").innerHTML = x2.toPrecision(4);
    }
    if (gnuplot.plot_axis_y2min != "none") {
	gnuplot.axis_y2min = (gnuplot.zoomed) ? gnuplot.zoom_axis_y2min : gnuplot.plot_axis_y2min;
	gnuplot.axis_y2max = (gnuplot.zoomed) ? gnuplot.zoom_axis_y2max : gnuplot.plot_axis_y2max;
	if (gnuplot.y2_mapping != undefined)
	    y2 = gnuplot.y2_mapping(y);
	else
	    y2 = gnuplot.axis_y2min - (gnuplot.ploty / (gnuplot.plot_ytop-gnuplot.plot_ybot)) * (gnuplot.axis_y2max - gnuplot.axis_y2min);
	if (document.getElementById(gnuplot.active_plot_name + "_y2"))
	    document.getElementById(gnuplot.active_plot_name + "_y2").innerHTML = y2.toPrecision(4);
    }
  
  var label_x, label_y;
  if (gnuplot.polar_mode) {
    var polar = gnuplot.convert_to_polar(x,y);
    label_x = "ang= " + polar.ang.toPrecision(4);
    label_y = "R= " + polar.r.toPrecision(4);
  } else {
    if (typeof(gnuplot.plot_timeaxis_x) == "string" && gnuplot.plot_timeaxis_x != "")
      label_x = gnuplot.datafmt(x);
    else
      label_x = x.toPrecision(4);
    if (typeof(gnuplot.plot_timeaxis_y) == "string" && gnuplot.plot_timeaxis_y != "")
      label_y = gnuplot.datafmt(y);
    else
      label_y = y.toPrecision(4);
  }

  if (document.getElementById(gnuplot.active_plot_name + "_x"))
      document.getElementById(gnuplot.active_plot_name + "_x").innerHTML = label_x;
  if (document.getElementById(gnuplot.active_plot_name + "_y"))
      document.getElementById(gnuplot.active_plot_name + "_y").innerHTML = label_y;

  // Echo the zoom box interactively
  if (gnuplot.zoom_in_progress) {
    // Clear previous box before drawing a new one
    if (gnuplot.full_canvas_image == null) {
      gnuplot.full_canvas_image = ctx.getImageData(0,0,canvas.width,canvas.height);
    } else {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.putImageData(gnuplot.full_canvas_image,0,0);
    }
    ctx.strokeStyle="rgba(128,128,128,0.60)";
    var x0 = gnuplot.plot_xmin + gnuplot.zoom_temp_plotx;
    var y0 = gnuplot.plot_ybot - gnuplot.zoom_temp_ploty;
    var w = gnuplot.plotx - gnuplot.zoom_temp_plotx;
    var h = -(gnuplot.ploty - gnuplot.zoom_temp_ploty);
    if (w<0) {x0 = x0 + w; w = -w;}
    if (h<0) {y0 = y0 + h; h = -h;}
    ctx.strokeRect(x0,y0,w,h);
  }

  // See if we are over a hypertext anchor point
  if (gnuplot.hypertext_list != "undefined" && gnuplot.hypertext_list.length > 0) {
    gnuplot.check_hypertext();
  }
}

gnuplot.check_hypertext = function()
{
  var nitems = gnuplot.hypertext_list.length;
  for (var i=0; i<nitems; i++) {
    var linkx = gnuplot.hypertext_list[i].x / 10.;
    var linky = gnuplot.hypertext_list[i].y / 10.;
    if (gnuplot.zoomed) {
      var zoom = gnuplot.zoomXY(linkx,linky);
      linkx = zoom.x; linky = zoom.y;
    }
    var delx = Math.abs(gnuplot.mousex - linkx);
    var dely = Math.abs(gnuplot.mousey - linky);
    var w = gnuplot.hypertext_list[i].w / 20.;
    if (delx < w && dely < w) {
	if (i == gnuplot.on_hypertext)
	    break;
	gnuplot.on_hypertext = i;
	var text = gnuplot.hypertext_list[i].text;
	var lines = text.split('\v');
	var len = 0;
	if (lines.length <= 1) {
	    len = ctx.measureText("sans", 10, text);
	} else {
	    for (var l=0; l<lines.length; l++) {
		var ll = ctx.measureText("sans", 10, lines[l]);
		if (len < ll) len = ll;
	    }
	}
	ctx.fillStyle = "rgba(238,238,238,0.8)"
	ctx.fillRect(linkx+10, linky+4, len+8, 14*lines.length);
	ctx.drawText("sans", 10, linkx+14, linky+14, text);
	break;
    }
  } 
  if (i == nitems && gnuplot.on_hypertext >= 0) {
    gnuplot.on_hypertext = -1;
    ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
    gnuplot.display_is_uptodate = false;
    gnuplot_canvas();
  }
}

gnuplot.datafmt = function (x)
{
  if (gnuplot.plot_timeaxis_x == "DMS") {
    return gnuplot.convert_to_DMS(x);
  }

  // gnuplot 4.6 used (x + 946684800)
  gnuplot.axisdate.setTime(1000. * x);

  if (gnuplot.plot_timeaxis_x == "DateTime") {
    return gnuplot.axisdate.toUTCString();
  } 
  if (gnuplot.plot_timeaxis_x == "Date") {
    var year = gnuplot.axisdate.getUTCFullYear();
    var month = gnuplot.axisdate.getUTCMonth();
    var date = gnuplot.axisdate.getUTCDate();
    return (" " + date).slice (-2) + "/"
         + ("0" + (month+1)).slice (-2) + "/"
	 + year;
  } 
  if (gnuplot.plot_timeaxis_x == "Time") {
    var hour = gnuplot.axisdate.getUTCHours();
    var minute = gnuplot.axisdate.getUTCMinutes();
    var second = gnuplot.axisdate.getUTCSeconds();
    return ("0" + hour).slice (-2) + ":"
         + ("0" + minute).slice (-2) + ":"
         + ("0" + second).slice (-2);
  }
}

gnuplot.convert_to_DMS = function (x)
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

gnuplot.convert_to_polar = function (x,y)
{
    var polar = new Object;
    var phi, r;
    phi = Math.atan2(y,x);
    if (gnuplot.plot_logaxis_r) 
        r = Math.exp( (x/Math.cos(phi) + Math.log(gnuplot.plot_axis_rmin)/Math.LN10) * Math.LN10);
    else
        r = x/Math.cos(phi) + gnuplot.plot_axis_rmin;
    polar.ang = phi * 180./Math.PI;
    polar.r = r;
    return polar;
}

gnuplot.saveclick = function (event)
{
  gnuplot.mouse_update(event);
  var button, label_x, label_y;
  
  // Limit tracking to the interior of the plot
  if (gnuplot.plotx < 0 || gnuplot.ploty < 0) return;
  if (gnuplot.mousex > gnuplot.plot_xmax || gnuplot.mousey < gnuplot.plot_ytop) return;

  if (event.which == null) 	/* IE case */
    button= (event.button < 2) ? "LEFT" : ((event.button == 4) ? "MIDDLE" : "RIGHT");
  else				/* All others */
    button= (event.which < 2) ? "LEFT" : ((event.which == 2) ? "MIDDLE" : "RIGHT");

  if (button == "LEFT") {
    ctx.strokeStyle="black";
    ctx.strokeRect(gnuplot.mousex, gnuplot.mousey, 1, 1);
    if (typeof(gnuplot.plot_timeaxis_x) == "string" && gnuplot.plot_timeaxis_x != "")
      label_x = gnuplot.datafmt(x);
    else
      label_x = x.toPrecision(4);
    if (typeof(gnuplot.plot_timeaxis_y) == "string" && gnuplot.plot_timeaxis_y != "")
      label_y = gnuplot.datafmt(y);
    else
      label_y = y.toPrecision(4);
    click = " " + label_x + ", " + label_y;
    ctx.drawText("sans", 9, gnuplot.mousex, gnuplot.mousey, click);
  }

  // Save starting corner of zoom box
  else {
    gnuplot.zoom_temp_xmin = x;
    gnuplot.zoom_temp_ymin = y;
    if (gnuplot.plot_axis_x2min != "none") gnuplot.zoom_temp_x2min = x2;
    if (gnuplot.plot_axis_y2min != "none") gnuplot.zoom_temp_y2min = y2;
    // Only used to echo the zoom box interactively
    gnuplot.zoom_temp_plotx = gnuplot.plotx;
    gnuplot.zoom_temp_ploty = gnuplot.ploty;
    gnuplot.zoom_in_progress = true;
    gnuplot.full_canvas_image = null;
  }
  return false; // Nobody else should respond to this event
}

gnuplot.zoom_in = function (event)
{
  if (!gnuplot.zoom_in_progress)
    return false;

  gnuplot.mouse_update(event);
 
  var button;

  if (event.which == null) 	/* IE case */
    button= (event.button < 2) ? "LEFT" : ((event.button == 4) ? "MIDDLE" : "RIGHT");
  else				/* All others */
    button= (event.which < 2) ? "LEFT" : ((event.which == 2) ? "MIDDLE" : "RIGHT");

  // Save ending corner of zoom box
  if (button != "LEFT") {
    if (x > gnuplot.zoom_temp_xmin) {
        gnuplot.zoom_axis_xmin = gnuplot.zoom_temp_xmin;
	gnuplot.zoom_axis_xmax = x;
	if (gnuplot.plot_axis_x2min != "none") {
            gnuplot.zoom_axis_x2min = gnuplot.zoom_temp_x2min;
	    gnuplot.zoom_axis_x2max = x2;
	}
    } else {
        gnuplot.zoom_axis_xmin = x;
	gnuplot.zoom_axis_xmax = gnuplot.zoom_temp_xmin;
	if (gnuplot.plot_axis_x2min != "none") {
            gnuplot.zoom_axis_x2min = x2;
	    gnuplot.zoom_axis_x2max = gnuplot.zoom_temp_x2min;
	}
    }
    if (y > gnuplot.zoom_temp_ymin) {
        gnuplot.zoom_axis_ymin = gnuplot.zoom_temp_ymin;
	gnuplot.zoom_axis_ymax = y;
	if (gnuplot.plot_axis_y2min != "none") {
            gnuplot.zoom_axis_y2min = gnuplot.zoom_temp_y2min;
	    gnuplot.zoom_axis_y2max = y2;
	}
    } else {
        gnuplot.zoom_axis_ymin = y;
	gnuplot.zoom_axis_ymax = gnuplot.zoom_temp_ymin;
	if (gnuplot.plot_axis_y2min != "none") {
            gnuplot.zoom_axis_y2min = y2;
	    gnuplot.zoom_axis_y2max = gnuplot.zoom_temp_y2min;
	}
    }
    gnuplot.zoom_axis_width = gnuplot.zoom_axis_xmax - gnuplot.zoom_axis_xmin;
    gnuplot.zoom_axis_height = gnuplot.zoom_axis_ymax - gnuplot.zoom_axis_ymin;
    gnuplot.zoom_in_progress = false;
    gnuplot.rezoom(event);
  }
  return false; // Nobody else should respond to this event
}

gnuplot.toggle_grid = function (e)
{
  if (!gnuplot.grid_lines) gnuplot.grid_lines = true;
  else gnuplot.grid_lines = false;
  ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
  gnuplot.display_is_uptodate = false;
  gnuplot_canvas();
}

gnuplot.toggle_zoom_text = function (e)
{
  if (!gnuplot.zoom_text) gnuplot.zoom_text = true;
  else gnuplot.zoom_text = false;
  ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
  gnuplot.display_is_uptodate = false;
  gnuplot_canvas();
}

gnuplot.rezoom = function (e)
{
  if (gnuplot.zoom_axis_width > 0)
    gnuplot.zoomed = true;
  ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
  gnuplot.display_is_uptodate = false;
  gnuplot_canvas();
}

gnuplot.unzoom = function (e)
{
  gnuplot.zoomed = false;
  ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
  gnuplot.display_is_uptodate = false;
  gnuplot_canvas();
}

gnuplot.zoomXY = function(x,y)
{
  var zoom = new Object;
  var xreal, yreal;

  zoom.x = x; zoom.y = y; zoom.clip = false;

  if (gnuplot.plot_logaxis_x != 0) {
	xreal = Math.log(gnuplot.plot_axis_xmax) - Math.log(gnuplot.plot_axis_xmin);
	xreal = Math.log(gnuplot.plot_axis_xmin) + (x - gnuplot.plot_xmin) * xreal/gnuplot.plot_width;
	zoom.x = Math.log(gnuplot.zoom_axis_xmax) - Math.log(gnuplot.zoom_axis_xmin);
	zoom.x = gnuplot.plot_xmin + (xreal - Math.log(gnuplot.zoom_axis_xmin)) * gnuplot.plot_width/zoom.x;
  } else {
	xreal = gnuplot.plot_axis_xmin + (x - gnuplot.plot_xmin) * (gnuplot.plot_axis_width/gnuplot.plot_width);
	zoom.x = gnuplot.plot_xmin + (xreal - gnuplot.zoom_axis_xmin) * (gnuplot.plot_width/gnuplot.zoom_axis_width);
  }
  if (gnuplot.plot_logaxis_y != 0) {
	yreal = Math.log(gnuplot.plot_axis_ymax) - Math.log(gnuplot.plot_axis_ymin);
	yreal = Math.log(gnuplot.plot_axis_ymin) + (gnuplot.plot_ybot - y) * yreal/gnuplot.plot_height;
	zoom.y = Math.log(gnuplot.zoom_axis_ymax) - Math.log(gnuplot.zoom_axis_ymin);
	zoom.y = gnuplot.plot_ybot - (yreal - Math.log(gnuplot.zoom_axis_ymin)) * gnuplot.plot_height/zoom.y;
  } else {
	yreal = gnuplot.plot_axis_ymin + (gnuplot.plot_ybot - y) * (gnuplot.plot_axis_height/gnuplot.plot_height);
	zoom.y = gnuplot.plot_ybot - (yreal - gnuplot.zoom_axis_ymin) * (gnuplot.plot_height/gnuplot.zoom_axis_height);
  }

  // Report unclipped coords also
  zoom.xraw = zoom.x; zoom.yraw = zoom.y;

  // Limit the zoomed plot to the original plot area
  if (x > gnuplot.plot_xmax) {
    zoom.x = x;
    if (gnuplot.plot_axis_y2min == "none") {
      zoom.y = y;
      return zoom;
    }
    if (gnuplot.plot_ybot <= y && y <= gnuplot.plot_ybot + 15)
      zoom.clip = true;
  }

  else if (x < gnuplot.plot_xmin)
    zoom.x = x;
  else if (zoom.x < gnuplot.plot_xmin)
    { zoom.x = gnuplot.plot_xmin; zoom.clip = true; }
  else if (zoom.x > gnuplot.plot_xmax)
    { zoom.x = gnuplot.plot_xmax; zoom.clip = true; }

  if (y < gnuplot.plot_ytop) {
    zoom.y = y;
    if (gnuplot.plot_axis_x2min == "none") {
      zoom.x = x; zoom.clip = false;
      return zoom;
    }
  }

  else if (y > gnuplot.plot_ybot)
    zoom.y = y;
  else if (zoom.y > gnuplot.plot_ybot)
    { zoom.y = gnuplot.plot_ybot; zoom.clip = true; }
  else if (zoom.y < gnuplot.plot_ytop)
    { zoom.y = gnuplot.plot_ytop; zoom.clip = true; }

  return zoom;
}

gnuplot.zoomW = function (w) { return (w*gnuplot.plot_axis_width/gnuplot.zoom_axis_width); }
gnuplot.zoomH = function (h) { return (h*gnuplot.plot_axis_height/gnuplot.zoom_axis_height); }

gnuplot.popup_help = function(URL) {
    if (typeof(URL) != "string") {
	if (typeof(gnuplot.help_URL) == "string") 
	    URL = gnuplot.help_URL;
	else
	    return;
    }
    // FIXME: Placeholder for useful action
    if (URL != "")
	window.open (URL,"gnuplot help");
}

gnuplot.toggle_plot = function(plotid) {
    if (typeof(gnuplot["hide_"+plotid]) == "unknown")
    	gnuplot["hide_"+plotid] = false;
    gnuplot["hide_"+plotid] = !gnuplot["hide_"+plotid];
    ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
    gnuplot.display_is_uptodate = false;
    gnuplot_canvas();
}

gnuplot.do_hotkey = function(event) {
    keychar = String.fromCharCode(event.charCode ? event.charCode : event.keyCode);
    switch (keychar) {
    case 'e':	ctx.clearRect(0,0,gnuplot.plot_term_xmax,gnuplot.plot_term_ymax);
		gnuplot.display_is_uptodate = false;
		gnuplot_canvas();
		break;
    case 'g':	gnuplot.toggle_grid();
		break;
    case 'n':	gnuplot.rezoom();
		break;
    case 'r':
		ctx.lineWidth = 0.5;
		ctx.strokeStyle="rgba(128,128,128,0.50)";
		ctx.moveTo(gnuplot.plot_xmin, gnuplot.mousey); ctx.lineTo(gnuplot.plot_xmax, gnuplot.mousey);
		ctx.moveTo(gnuplot.mousex, gnuplot.plot_ybot); ctx.lineTo(gnuplot.mousex, gnuplot.plot_ytop);
		ctx.stroke();
		break;
    case 'p':
    case 'u':	gnuplot.unzoom();
		break;
    case '':	zoom_in_progress = false;
		break;

// Arrow keys
    case '%':	// ctx.drawText("sans", 10, gnuplot.mousex, gnuplot.mousey, "<");
		break;
    case '\'':	// ctx.drawText("sans", 10, gnuplot.mousex, gnuplot.mousey, ">");
		break;
    case '&':	// ctx.drawText("sans", 10, gnuplot.mousex, gnuplot.mousey, "^");
		break;
    case '(':	// ctx.drawText("sans", 10, gnuplot.mousex, gnuplot.mousey, "v");
		break;

    default:	ctx.drawText("sans", 10, gnuplot.mousex, gnuplot.mousey, keychar);
		return true; // Let someone else handle it
		break;
    }
    return false; // Nobody else should respond to this keypress
}
