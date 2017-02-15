/*
 * $Id: gnuplot_common.js,v 1.10 2012/05/03 20:35:22 sfeam Exp $
 */
// Shared routines for gnuplot's HTML5 canvas terminal driver.

var gnuplot = { };

gnuplot.common_version = "3 May 2012";

gnuplot.L = function (x,y) {
  if (gnuplot.zoomed) {
    var zoom = gnuplot.zoomXY(x/10.0,y/10.0);
    ctx.lineTo(zoom.x,zoom.y);
  } else
    ctx.lineTo(x/10.0,y/10.0);
}
gnuplot.M = function (x,y) {
  if (gnuplot.zoomed) {
    var zoom = gnuplot.zoomXY(x/10.0,y/10.0);
    ctx.moveTo(zoom.x,zoom.y);
  } else
    ctx.moveTo(x/10.0,y/10.0);
}
gnuplot.R = function (x,y,w,h) {
  if (gnuplot.zoomed) {
    var dx, dy, dw, dh;
    var zoom = gnuplot.zoomXY(x/10.0,y/10.0);
    if (zoom.x >= gnuplot.plot_xmax) return;
    if (zoom.y >= gnuplot.plot_ybot) return;
    dx = zoom.x; dy = zoom.y;
    zoom = gnuplot.zoomXY((x+w)/10.,(y+h)/10.);
    if (zoom.xraw <= gnuplot.plot_xmin) return;
    if (zoom.yraw <= gnuplot.plot_ytop) return;
    dw = zoom.x - dx; dh = zoom.y -dy;
    ctx.fillRect(dx, dy, dw, dh);
  } else
    ctx.fillRect(x/10.0, y/10.0, w/10.0, h/10.0);
}
gnuplot.T = function (x,y,fontsize,justify,string) {
  var xx = x/10.0; var yy = y/10.0;
  if (gnuplot.zoomed) {
    var zoom = gnuplot.zoomXY(xx,yy);
    if (zoom.clip) return;
    xx = zoom.x; yy = zoom.y;
    if (gnuplot.plot_xmin < xx && xx < gnuplot.plot_xmax && gnuplot.plot_ybot > yy && yy > gnuplot.plot_ytop)
      if ((typeof(gnuplot.zoom_text) != "undefined") && (gnuplot.zoom_text == true))
	fontsize = Math.sqrt(gnuplot.zoomW(fontsize)*gnuplot.zoomH(fontsize));
  }
  if (justify=="") ctx.drawText("sans", fontsize, xx, yy, string);
  else if (justify=="Right") ctx.drawTextRight("sans", fontsize, xx, yy, string);
  else if (justify=="Center") ctx.drawTextCenter("sans", fontsize, xx, yy, string);
}
gnuplot.TR = function (x,y,angle,fontsize,justify,string) {
  var xx = x/10.0; var yy = y/10.0;
  if (gnuplot.zoomed) {
    var zoom = gnuplot.zoomXY(xx,yy);
    if (zoom.clip) return;
    xx = zoom.x; yy = zoom.y;
    if (gnuplot.plot_xmin < xx && xx < gnuplot.plot_xmax && gnuplot.plot_ybot > yy && yy > gnuplot.plot_ytop)
      if ((typeof(gnuplot.zoom_text) != "undefined") && (gnuplot.zoom_text == true))
	fontsize = Math.sqrt(gnuplot.zoomW(fontsize)*gnuplot.zoomH(fontsize));
  }
  ctx.save();
  ctx.translate(xx,yy);
  ctx.rotate(angle * Math.PI / 180);
  if (justify=="") ctx.drawText("sans", fontsize, 0, 0, string);
  else if (justify=="Right") ctx.drawTextRight("sans", fontsize, 0, 0, string);
  else if (justify=="Center") ctx.drawTextCenter("sans", fontsize, 0, 0, string);
  ctx.restore();
}
gnuplot.bp = function (x,y) // begin polygon
    { ctx.beginPath(); gnuplot.M(x,y); }
gnuplot.cfp = function () // close and fill polygon
    { ctx.closePath(); ctx.fill(); }
gnuplot.cfsp = function () // close and fill polygon with stroke color
    { ctx.closePath(); ctx.fillStyle = ctx.strokeStyle; ctx.stroke(); ctx.fill(); }
gnuplot.Dot = function (x,y) {
    var xx = x; var yy = y;
    if (gnuplot.zoomed) {zoom = gnuplot.zoomXY(xx,yy); xx = zoom.x; yy = zoom.y; if (zoom.clip) return;}
    ctx.strokeRect(xx,yy,0.5,0.5);
}
gnuplot.Pt = function (N,x,y,w) {
    var xx = x; var yy = y;
    if (gnuplot.zoomed) {var zoom = gnuplot.zoomXY(xx,yy); xx = zoom.x; yy = zoom.y; if (zoom.clip) return;}
    if (w==0) return;
    switch (N)
    {
    case 0:
	ctx.beginPath();
	ctx.moveTo(xx-w,yy); ctx.lineTo(xx+w,yy);
	ctx.moveTo(xx,yy-w); ctx.lineTo(xx,yy+w);
	ctx.stroke();
	break;
    case 1:
	var ww = w * 3/4;
	ctx.beginPath();
	ctx.moveTo(xx-ww,yy-ww); ctx.lineTo(xx+ww,yy+ww);
	ctx.moveTo(xx+ww,yy-ww); ctx.lineTo(xx-ww,yy+ww);
	ctx.stroke();
	break;
    case 2:
	gnuplot.Pt(0,x,y,w); gnuplot.Pt(1,x,y,w);
	break;
    case 3:
	ctx.strokeRect(xx-w/2,yy-w/2,w,w);
	break;
    case 4:
	ctx.save(); ctx.strokeRect(xx-w/2,yy-w/2,w,w); ctx.restore();
	ctx.fillRect(xx-w/2,yy-w/2,w,w);
	break;
    case 5:
	ctx.beginPath(); ctx.arc(xx,yy,w/2,0,Math.PI*2,true); ctx.stroke();
	break;
    default:
    case 6:
	ctx.beginPath(); ctx.arc(xx,yy,w/2,0,Math.PI*2,true); ctx.fill();
	break;
    case 7:
	ctx.beginPath();
	ctx.moveTo(xx,yy-w); ctx.lineTo(xx-w,yy+w/2); ctx.lineTo(xx+w,yy+w/2);
	ctx.closePath();
	ctx.stroke();
	break;
    case 8:
	ctx.beginPath();
	ctx.moveTo(xx,yy-w); ctx.lineTo(xx-w,yy+w/2); ctx.lineTo(xx+w,yy+w/2);
	ctx.closePath();
	ctx.fill();
	break;
    }
}

// Zoomable image
gnuplot.ZI = function (image, m, n, x1, y1, x2, y2) {
  if (gnuplot.zoomed) {
    var sx, sy, sw, sh, dx, dy, dw, dh;

    var zoom = gnuplot.zoomXY(x1/10.0,y1/10.0);
    if (zoom.x >= gnuplot.plot_xmax) return;
    if (zoom.y >= gnuplot.plot_ybot) return;
    x1raw = zoom.xraw; y1raw = zoom.yraw;
    dx = zoom.x; dy = zoom.y;

    zoom = gnuplot.zoomXY((x2)/10.,(y2)/10.);
    if (zoom.xraw <= gnuplot.plot_xmin) return;
    if (zoom.yraw <= gnuplot.plot_ytop) return;
    var x2raw = zoom.xraw; var y2raw = zoom.yraw;
    dw = zoom.x - dx;  dh = zoom.y - dy;

    // FIXME: This is sometimes flaky. Needs integer truncation?
    sx = 0; sy = 0; sw = m; sh = n;
    if (x1raw < dx) sx = m * (dx - x1raw) / (x2raw - x1raw);
    if (y1raw < dy) sy = n * (dy - y1raw) / (y2raw - y1raw);
    if (x2raw > zoom.x)
	sw = m * (1. - ((x2raw - zoom.x) / (x2raw - x1raw)));
    if (y2raw > zoom.y)
	sh = n * (1. - ((y2raw - zoom.y) / (y2raw - y1raw)));
    sw = sw - sx; sh = sh - sy;

    ctx.drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh);
} else
    ctx.drawImage(image, x1/10.0, y1/10.0, (x2-x1)/10.0, (y2-y1)/10.0);
}

// These methods are place holders that are loaded by gnuplot_dashedlines.js

gnuplot.dashtype  = function (dt) {} ;
gnuplot.dashstart = function (x,y) {gnuplot.M(x,y);} ;
gnuplot.dashstep  = function (x,y) {gnuplot.L(x,y);} ;
gnuplot.pattern   = [];
