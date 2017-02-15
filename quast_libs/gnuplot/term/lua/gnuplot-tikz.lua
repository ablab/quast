--[[

  This is the execution script for the `Lua generic terminal' driver.

  This script provides an interface to the PGF/TikZ package for LaTeX.


  Copyright 2008    Peter Hedwig <peter@affenbande.org>



  Permission to use, copy, and distribute this software and its
  documentation for any purpose with or without fee is hereby granted,
  provided that the above copyright notice appear in all copies and
  that both that copyright notice and this permission notice appear
  in supporting documentation.

  Permission to modify the software is granted, but not the right to
  distribute the complete modified source code.  Modifications are to
  be distributed as patches to the released version.  Permission to
  distribute binaries produced by compiling modified sources is granted,
  provided you
    1. distribute the corresponding source modifications from the
     released version in the form of a patch file along with the binaries,
    2. add special version identification to distinguish your version
     in addition to the base release version number,
    3. provide your name and address as the primary contact for the
     support of your modified version, and
    4. retain our contact information in regard to use of the base
     software.
  Permission to distribute the released version of the source code along
  with corresponding source modifications in the form of a patch file is
  granted with same provisions 2 through 4 for binary distributions.

  This software is provided "as is" without express or implied warranty
  to the extent permitted by applicable law.



  $Date: 2015/10/17 05:23:22 $
  $Author: sfeam $
  $Rev: 100 $

]]--



--[[
 
 `term'   gnuplot term_api -> local interface
 `gp'     local -> gnuplot interface

  are both initialized by the terminal

]]--


--
-- internal variables
--

local pgf = {}
local gfx = {}

-- the terminal default size in cm
pgf.DEFAULT_CANVAS_SIZE_X = 12.5
pgf.DEFAULT_CANVAS_SIZE_Y = 8.75
-- tic default size in cm
pgf.DEFAULT_TIC_SIZE = 0.18
-- the terminal resolution in "dots" per cm.
pgf.DEFAULT_RESOLUTION = 1000
-- default font size in TeX pt
pgf.DEFAULT_FONT_SIZE = 10
-- default sizes for CM@10pt and default resolution
-- there is no need to adapt these values when changing 
-- pgf.DEFAULT_FONT_SIZE or pgf.DEFAULT_RESOLUTION !
pgf.DEFAULT_FONT_H_CHAR = 184
pgf.DEFAULT_FONT_V_CHAR = 308


pgf.STYLE_FILE_BASENAME = "gnuplot-lua-tikz"  -- \usepackage{gnuplot-lua-tikz}

pgf.REVISION = string.sub("$Rev: 100 $",7,-3)
pgf.REVISION_DATE = string.gsub("$Date: 2015/10/17 05:23:22 $",
                                "$Date: ([0-9]+).([0-9]+).([0-9]+) .*","%1/%2/%3")

pgf.styles = {}

-- the styles are used in conjunction with the 'tikzarrows'
-- and the style number directly corresponds to the used
-- angle in the gnuplot style definition
pgf.styles.arrows = {
   [1] = {"gp arrow 1", ">=latex"},
   [2] = {"gp arrow 2", ">=angle 90"},
   [3] = {"gp arrow 3", ">=angle 60"},
   [4] = {"gp arrow 4", ">=angle 45"},
   [5] = {"gp arrow 5", ">=o"},
   [6] = {"gp arrow 6", ">=*"},
   [7] = {"gp arrow 7", ">=diamond"},
   [8] = {"gp arrow 8", ">=open diamond"},
   [9] = {"gp arrow 9", ">={]}"},
  [10] = {"gp arrow 10", ">={[}"},
  [11] = {"gp arrow 11", ">=)"},
  [12] = {"gp arrow 12", ">=("}
}

-- plot styles are corresponding with linetypes and must have the same number of entries
-- see option 'tikzplot' for usage
pgf.styles.plotstyles_axes = {
  [1] = {"gp plot axes", ""},
  [2] = {"gp plot border", ""},
}

pgf.styles.plotstyles = {
  [1] = {"gp plot 0", "smooth"},
  [2] = {"gp plot 1", "smooth"},
  [3] = {"gp plot 2", "smooth"},
  [4] = {"gp plot 3", "smooth"},
  [5] = {"gp plot 4", "smooth"},
  [6] = {"gp plot 5", "smooth"},
  [7] = {"gp plot 6", "smooth"},
  [8] = {"gp plot 7", "smooth"}
}

pgf.styles.linetypes_axes = {
  [1] = {"gp lt axes", "dotted"},  -- An lt of -1 is used for the X and Y axes.  
  [2] = {"gp lt border", "solid"}, -- An lt of -2 is used for the border of the plot.
}

pgf.styles.linetypes = {
  [1] = {"gp lt plot 0", ""}, -- first graph
  [2] = {"gp lt plot 1", ""}, -- second ...
  [3] = {"gp lt plot 2", ""},
  [4] = {"gp lt plot 3", ""},
  [5] = {"gp lt plot 4", ""},
  [6] = {"gp lt plot 5", ""},
  [7] = {"gp lt plot 6", ""},
  [8] = {"gp lt plot 7", ""}
}

pgf.styles.dashtypes_axes = {
  [1] = {"gp dt solid", "solid"},
  [2] = {"gp dt axes", "dotted"}
}

pgf.styles.dashtypes = {
  [0] = {"gp dt 0", "solid"}, 
  [1] = {"gp dt 1", "solid"},
  [2] = {"gp dt 2", "dash pattern=on 7.5*\\gpdashlength off 7.5*\\gpdashlength"},
  [3] = {"gp dt 3", "dash pattern=on 3.75*\\gpdashlength off 5.625*\\gpdashlength"},
  [4] = {"gp dt 4", "dash pattern=on 1*\\gpdashlength off 2.8125*\\gpdashlength"},
  [5] = {"gp dt 5", "dash pattern=on 11.25*\\gpdashlength off 3.75*\\gpdashlength on 1*\\gpdashlength off 3.75*\\gpdashlength"},
  [6] = {"gp dt 6", "dash pattern=on 5.625*\\gpdashlength off 5.625*\\gpdashlength on 1*\\gpdashlength off 5.625*\\gpdashlength"},
  [7] = {"gp dt 7", "dash pattern=on 3.75*\\gpdashlength off 3.75*\\gpdashlength on 3.75*\\gpdashlength off 11.25*\\gpdashlength"},
  [8] = {"gp dt 8", "dash pattern=on 1*\\gpdashlength off 3.75*\\gpdashlength on 11.25*\\gpdashlength off 3.75*\\gpdashlength on 1*\\gpdashlength off 3.75*\\gpdashlength"}
}

-- corresponds to pgf.styles.linetypes
pgf.styles.lt_colors_axes = {
  [1] = {"gp lt color axes", "black!30"},
  [2] = {"gp lt color border", "black"},
}

pgf.styles.lt_colors = {
  [1] = {"gp lt color 0", "red"},
  [2] = {"gp lt color 1", "green"},
  [3] = {"gp lt color 2", "blue"},
  [4] = {"gp lt color 3", "magenta"},
  [5] = {"gp lt color 4", "cyan"},
  [6] = {"gp lt color 5", "yellow"},
  [7] = {"gp lt color 6", "orange"},
  [8] = {"gp lt color 7", "purple"}
}

pgf.styles.patterns = {
  [1] = {"gp pattern 0", "white"},
  [2] = {"gp pattern 1", "pattern=north east lines"},
  [3] = {"gp pattern 2", "pattern=north west lines"},
  [4] = {"gp pattern 3", "pattern=crosshatch"},
  [5] = {"gp pattern 4", "pattern=grid"},
  [6] = {"gp pattern 5", "pattern=vertical lines"},
  [7] = {"gp pattern 6", "pattern=horizontal lines"},
  [8] = {"gp pattern 7", "pattern=dots"},
  [9] = {"gp pattern 8", "pattern=crosshatch dots"},
 [10] = {"gp pattern 9", "pattern=fivepointed stars"},
 [11] = {"gp pattern 10", "pattern=sixpointed stars"},
 [12] = {"gp pattern 11", "pattern=bricks"}
}


pgf.styles.plotmarks = {
  [1] = {"gp mark 0", "mark size=.5\\pgflinewidth,mark=*"}, -- point (-1)
  [2] = {"gp mark 1", "mark=+"},
  [3] = {"gp mark 2", "mark=x"},
  [4] = {"gp mark 3", "mark=star"},
  [5] = {"gp mark 4", "mark=square"},
  [6] = {"gp mark 5", "mark=square*"},
  [7] = {"gp mark 6", "mark=o"},
  [8] = {"gp mark 7", "mark=*"},
  [9] = {"gp mark 8", "mark=triangle"},
 [10] = {"gp mark 9", "mark=triangle*"},
 [11] = {"gp mark 10", "mark=triangle,every mark/.append style={rotate=180}"},
 [12] = {"gp mark 11", "mark=triangle*,every mark/.append style={rotate=180}"},
 [13] = {"gp mark 12", "mark=diamond"},
 [14] = {"gp mark 13", "mark=diamond*"},
 [15] = {"gp mark 14", "mark=otimes"},
 [16] = {"gp mark 15", "mark=oplus"}
}  

--[[===============================================================================================

    helper functions

]]--===============================================================================================

-- from the Lua wiki
explode = function(div,str)
  if (div=='') then return false end
  local pos,arr = 0,{}
  local trim = function(s) return (string.gsub(s,"^%s*(.-)%s*$", "%1")) end
  -- for each divider found
  for st,sp in function() return string.find(str,div,pos,true) end do
    table.insert(arr, trim(string.sub(str,pos,st-1))) -- Attach chars left of current divider
    pos = sp + 1 -- Jump past current divider
  end
  table.insert(arr, trim(string.sub(str,pos))) -- Attach chars right of last divider
  return arr
end



--[[===============================================================================================

  The PGF/TikZ output routines

]]--===============================================================================================


pgf.transform_xcoord = function(coord)
  return (coord+gfx.origin_xoffset)*gfx.scalex
end

pgf.transform_ycoord = function(coord)
  return (coord+gfx.origin_yoffset)*gfx.scaley
end

pgf.format_coord = function(xc, yc)
  return string.format("%.3f,%.3f", pgf.transform_xcoord(xc), pgf.transform_ycoord(yc))
end

pgf.write_doc_begin = function(preamble)
  gp.write(gfx.format[gfx.opt.tex_format].docheader)
  gp.write(preamble)
  gp.write(gfx.format[gfx.opt.tex_format].begindocument)
end

pgf.write_doc_end = function()
    gp.write(gfx.format[gfx.opt.tex_format].enddocument)
end

pgf.write_graph_begin = function (font, noenv)
  local global_opt = "" -- unused
  if gfx.opt.full_doc then
    gp.write(gfx.format[gfx.opt.tex_format].beforetikzpicture)
  end
  if noenv then
    gp.write("%% ") -- comment out
  end
  gp.write(string.format("%s[gnuplot%s]\n", gfx.format[gfx.opt.tex_format].begintikzpicture, global_opt))
  gp.write(string.format("%%%% generated with GNUPLOT %sp%s (%s; terminal rev. %s, script rev. %s)\n",
      term.gp_version, term.gp_patchlevel, _VERSION, string.sub(term.lua_term_revision,7,-3), pgf.REVISION))
  if not gfx.opt.notimestamp then
    gp.write(string.format("%%%% %s\n", os.date()))
  end
  if font ~= "" then
    gp.write(string.format("\\tikzset{every node/.append style={font=%s}}\n", font))
  end
  if gfx.opt.fontscale ~= nil then
    gp.write(string.format("\\tikzset{every node/.append style={scale=%.2f}}\n", gfx.opt.fontscale))
  end
  if not gfx.opt.lines_colored then
    gp.write("\\gpmonochromelines\n")
  end
  if gfx.opt.bgcolor ~= nil then
    gp.write(string.format("\\gpsetbgcolor{%.3f,%.3f,%.3f}\n", gfx.opt.bgcolor[1], gfx.opt.bgcolor[2], gfx.opt.bgcolor[3]))
  end
  if gfx.opt.dashlength ~= nil then
    gp.write(string.format("\\def\\gpdashlength{%.2f\\pgflinewidth}\n", gfx.opt.dashlength))
  end
end

pgf.write_graph_end = function(noenv)
  if noenv then
    gp.write("%% ") -- comment out
  end
  if gfx.opt.full_doc then
    gp.write(gfx.format[gfx.opt.tex_format].beforeendtikzpicture)
  end
  gp.write(gfx.format[gfx.opt.tex_format].endtikzpicture .. "\n")
  if gfx.opt.full_doc then
    gp.write(gfx.format[gfx.opt.tex_format].aftertikzpicture)
  end
end

pgf.draw_path = function(t)
  local use_plot = false
  local c_str = '--'

  -- is the current linetype in the list of plots?
  if #gfx.opt.plot_list > 0 then
    for k, v in pairs(gfx.opt.plot_list) do
      if gfx.linetype_idx_set == v  then
        use_plot = true
        c_str = ' '
        break
      end
    end
  end

  gp.write("\\draw[gp path")
  if gfx.opacity < 1.0 then
    gp.write(string.format(",opacity=%.3f", gfx.opacity))
  end
  gp.write("] ")
  if use_plot then
    gp.write("plot["..pgf.styles.plotstyles[((gfx.linetype_idx_set) % #pgf.styles.plotstyles)+1][1].."] coordinates {")
  end
  gp.write("("..pgf.format_coord(t[1][1], t[1][2])..")")
  for i = 2,#t-1 do
    -- pretty printing
    if (i % 6) == 0 then
      gp.write("%\n  ")
    end
    gp.write(c_str.."("..pgf.format_coord(t[i][1], t[i][2])..")")
  end
  if (#t % 6) == 0 then
    gp.write("%\n  ")
  end
  -- check for a cyclic path
  if (t[1][1] == t[#t][1]) and (t[1][2] == t[#t][2]) and (not use_plot) then
    gp.write("--cycle")
  else
    gp.write(c_str.."("..pgf.format_coord(t[#t][1], t[#t][2])..")")
  end
  if use_plot then
    gp.write("}")
  end
  gp.write(";\n")
end


pgf.draw_arrow = function(t, direction, headstyle)
  gp.write("\\draw[gp path")
  if direction ~= '' and direction ~= nil then
    gp.write(","..direction)
  end
  if headstyle > 0 then
    gp.write(",gp arrow "..headstyle)
  end
  if gfx.opacity < 1.0 then
    gp.write(string.format(",opacity=%.3f", gfx.opacity))
  end
  gp.write("]")
  gp.write("("..pgf.format_coord(t[1][1], t[1][2])..")")
  for i = 2,#t do
    if (i % 6) == 0 then
      gp.write("%\n  ")
    end
    gp.write("--("..pgf.format_coord(t[i][1], t[i][2])..")")
  end
  gp.write(";\n")
end


pgf.draw_points = function(t, pm)
  gp.write("\\gppoint{"..pm.."}{")
  for i,v in ipairs(t) do
      gp.write("("..pgf.format_coord(v[1], v[2])..")")
  end
  gp.write("}\n")
end


pgf.set_linetype = function(linetype)
  gp.write("\\gpsetlinetype{"..linetype.."}\n")
end


pgf.set_dashtype = function(dashtype)
  gp.write("\\gpsetdashtype{"..dashtype.."}\n")
end


pgf.set_color = function(color)
  gp.write("\\gpcolor{"..color.."}\n")
end


pgf.set_linewidth = function(width)
  gp.write(string.format("\\gpsetlinewidth{%.2f}\n", width))
end


pgf.set_pointsize = function(size)
  gp.write(string.format("\\gpsetpointsize{%.2f}\n", 4*size))
end


pgf.write_text_node = function(t, text, angle, justification, font)
  local node_options = justification
  if angle ~= 0 then
    node_options = node_options .. ",rotate=" .. angle
  end
  if font ~= '' then
    node_options = node_options .. ",font=" .. font
  end  
  node_name = ''
  if gfx.boxed_text then
     gfx.boxed_text_count = gfx.boxed_text_count + 1
     node_options = node_options .. ",inner sep=0pt"
     node_name = string.format("(gp boxed node %d)", gfx.boxed_text_count)
  end
  if gfx.opacity < 1.0 then
    node_options = node_options .. string.format(",text opacity=%.3f", gfx.opacity)
  end
  gp.write(string.format("\\node[%s]%s at (%s) {%s};\n", 
          node_options, node_name, pgf.format_coord(t[1], t[2]), text))
end


pgf.draw_fill = function(t, pattern, color, saturation, opacity)
  local fill_path = ''
  local fill_style = color
  
  if saturation < 100 then
    fill_style = fill_style .. ",color=.!"..saturation;
  end

  fill_path = fill_path .. '('..pgf.format_coord(t[1][1], t[1][2])..')'
  -- draw 2nd to n-1 corners
  for i = 2,#t-1 do
    if (i % 5) == 0 then
      -- pretty printing
      fill_path = fill_path .. "%\n    "
    end
    fill_path = fill_path .. '--('..pgf.format_coord(t[i][1], t[i][2])..')'
  end
  if (#t % 5) == 0 then
    gp.write("%\n  ")
  end
  -- draw last corner
  -- 'cycle' is just for the case that we want to draw a
  -- line around the filled area
  if (t[1][1] == t[#t][1]) and (t[1][2] == t[#t][2]) then -- cyclic
    fill_path = fill_path .. '--cycle'
  else
    fill_path = fill_path
          .. '--('..pgf.format_coord(t[#t][1], t[#t][2])..')--cycle'
  end
  
  if pattern == '' then
    -- solid fills
--    fill_style = 'color='..color
    if opacity < 100 then
      fill_style = fill_style..string.format(",opacity=%.2f", opacity/100)
    else
      -- fill_style = "" -- color ?
    end
  else
    -- pattern fills
    fill_style = fill_style..','..pattern..',pattern color=.'
  end
  local out = ''
  if (pattern ~= '') and (opacity == 100) then
    -- have to fill bg for opaque patterns
    gp.write("\\def\\gpfillpath{"..fill_path.."}\n"
          .. "\\gpfill{color=gpbgfillcolor} \\gpfillpath;\n"
          .. "\\gpfill{"..fill_style.."} \\gpfillpath;\n")
  else
    gp.write("\\gpfill{"..fill_style.."} "..fill_path..";\n")
  end
end
  
pgf.load_image_file = function(ll, ur, xfile)
  gp.write(string.format("\\gploadimage{%.3f}{%.3f}{%.3f}{%.3f}{%s}\n",
      pgf.transform_xcoord(ll[1]), pgf.transform_ycoord(ll[2]),
      (pgf.transform_xcoord(ur[1]) - pgf.transform_xcoord(ll[1])),
      (pgf.transform_ycoord(ur[2]) - pgf.transform_ycoord(ll[2])),
       xfile))
end

pgf.draw_raw_rgb_image = function(t, m, n, ll, ur, xfile)
  local gw = gp.write
  local sf = string.format
  local xs = sf("%.3f", pgf.transform_xcoord(ur[1]) - pgf.transform_xcoord(ll[1]))
  local ys = sf("%.3f", pgf.transform_ycoord(ur[2]) - pgf.transform_ycoord(ll[2]))
  gw("\\def\\gprawrgbimagedata{%\n  ")
  for cnt = 1,#t do
    gw(sf("%02x%02x%02x", 255*t[cnt][1]+0.5, 255*t[cnt][2]+0.5, 255*t[cnt][3]+0.5))
    if (cnt % 16) == 0 then
      gw("%\n  ")
    end
  end
  gw("}%\n")
  gw("\\gprawimage{rgb}{"..sf("%.3f", pgf.transform_xcoord(ll[1])).."}"
      .."{"..sf("%.3f", pgf.transform_ycoord(ll[2])).."}"
      .."{"..m.."}{"..n.."}{"..xs.."}{"..ys.."}{\\gprawrgbimagedata}{"..xfile.."}\n")
end

pgf.draw_raw_cmyk_image = function(t, m, n, ll, ur, xfile)
  local gw = gp.write
  local sf = string.format
  local min = math.min
  local max = math.max
  local mf = math.floor
  local UCRBG = {1,1,1,1} -- default corrections
  local rgb2cmyk255 = function(r,g,b)
    local c = 1-r
    local m = 1-g
    local y = 1-b
    local k = min(c,m,y)
    c = mf(255*min(1, max(0, c - UCRBG[1]*k))+0.5)
    m = mf(255*min(1, max(0, m - UCRBG[2]*k))+0.5)
    y = mf(255*min(1, max(0, y - UCRBG[3]*k))+0.5)
    k = mf(255*min(1, max(0,     UCRBG[4]*k))+0.5)
    return c,m,y,k
  end
  local xs = sf("%.3f", pgf.transform_xcoord(ur[1]) - pgf.transform_xcoord(ll[1]))
  local ys = sf("%.3f", pgf.transform_ycoord(ur[2]) - pgf.transform_ycoord(ll[2]))
  gw("\\def\\gprawcmykimagedata{%\n  ")
  for cnt = 1,#t do
    gw(sf("%02x%02x%02x%02x", rgb2cmyk255(t[cnt][1],t[cnt][2],t[cnt][3])))
    if (cnt % 12) == 0 then
      gw("%\n  ")
    end
  end
  gw("}%\n")
  gw("\\gprawimage{cmyk}{"..sf("%.3f", pgf.transform_xcoord(ll[1])).."}"
      .."{"..sf("%.3f", pgf.transform_ycoord(ll[2])).."}"
      .."{"..m.."}{"..n.."}{"..xs.."}{"..ys.."}{\\gprawcmykimagedata}{"..xfile.."}\n")
end

pgf.write_clipbox_begin = function (ll, ur)
  gp.write(gfx.format[gfx.opt.tex_format].beginscope.."\n")
  gp.write(string.format("\\clip (%s) rectangle (%s);\n",
      pgf.format_coord(ll[1],ll[2]),pgf.format_coord(ur[1],ur[2])))
end

pgf.write_clipbox_end = function()
  gp.write(gfx.format[gfx.opt.tex_format].endscope.."\n")
end

pgf.write_boundingbox = function(t, num)
  gp.write("%% coordinates of the plot area\n")
  gp.write("\\gpdefrectangularnode{gp plot "..num.."}{"
    ..string.format("\\pgfpoint{%.3fcm}{%.3fcm}", pgf.transform_xcoord(t.xleft), pgf.transform_ycoord(t.ybot)).."}{"
    ..string.format("\\pgfpoint{%.3fcm}{%.3fcm}", pgf.transform_xcoord(t.xright), pgf.transform_ycoord(t.ytop)).."}\n")
end

pgf.write_variables = function(t)
  gp.write("%% gnuplot variables\n")
  for k, v in pairs(t) do
    gp.write(string.format("\\gpsetvar{%s}{%s}\n",k,v))
  end
end

-- write style to seperate file, or whatever...
pgf.create_style = function()
  local name_common  = pgf.STYLE_FILE_BASENAME.."-common.tex"
  local name_latex   = pgf.STYLE_FILE_BASENAME..".sty"
  local name_tex     = pgf.STYLE_FILE_BASENAME..".tex"
  local name_context = "t-"..pgf.STYLE_FILE_BASENAME..".tex"

-- LaTeX

local f_latex   = io.open(name_latex, "w+")
f_latex:write([[
%%
%%  LaTeX wrapper for gnuplot-tikz style file
%%
\NeedsTeXFormat{LaTeX2e}
]])
f_latex:write("\\ProvidesPackage{"..pgf.STYLE_FILE_BASENAME.."}%\n")
f_latex:write("          ["..pgf.REVISION_DATE.." (rev. "..pgf.REVISION..") GNUPLOT Lua terminal style]\n\n")
f_latex:write([[
\RequirePackage{tikz}

\usetikzlibrary{arrows,patterns,plotmarks,backgrounds,fit}
]])
f_latex:write("\\input "..name_common.."\n")
f_latex:write([[

\endinput
]])
f_latex:close()

-- ConTeXt

local f_context = io.open(name_context, "w+")
f_context:write([[
%%
%%  ConTeXt wrapper for gnuplot-tikz style file
%%
\usemodule[tikz]

\usetikzlibrary[arrows,patterns,plotmarks,backgrounds]

\edef\tikzatcode{\the\catcode`\@}
\edef\tikzbarcode{\the\catcode`\|}
\edef\tikzexclaimcode{\the\catcode`\!}
\catcode`\@=11
\catcode`\|=12
\catcode`\!=12

]])
f_context:write("\\input "..name_common.."\n")
f_context:write([[

\catcode`\@=\tikzatcode
\catcode`\|=\tikzbarcode
\catcode`\!=\tikzexclaimcode

\endinput
]])
f_context:close()


-- plain TeX

local f_tex     = io.open(name_tex, "w+")
f_tex:write([[
%%
%%  plain TeX wrapper for gnuplot-tikz style file
%%
\input tikz.tex
\usetikzlibrary{arrows,patterns,plotmarks,backgrounds}

\edef\tikzatcode{\the\catcode`\@}
\catcode`\@=11

]])
f_tex:write("\\input "..name_common.."\n\n")
f_tex:write([[

\catcode`\@=\tikzatcode

\endinput
]])
f_tex:close()

-- common

local f = io.open(name_common, "w+")
f:write([[
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%
%%     Common style file for TeX, LaTeX and ConTeXt
%%  
%%  It is associated with the 'gnuplot.lua' script, and usually generated
%%  automatically. So take care whenever you make any changes!
%%

% check for the correct TikZ version
\def\gpchecktikzversion#1.#2\relax{%
\ifnum#1<2%
  \errmessage{PGF/TikZ version >= 2.0 is required!}%
\fi}
\expandafter\gpchecktikzversion\pgfversion\relax

% FIXME: is there a more elegant way to determine the output format?

\def\pgfsysdriver@a{pgfsys-dvi.def}       % ps
\def\pgfsysdriver@b{pgfsys-dvipdfm.def}   % pdf
\def\pgfsysdriver@c{pgfsys-dvipdfmx.def}  % pdf
\def\pgfsysdriver@d{pgfsys-dvips.def}     % ps
\def\pgfsysdriver@e{pgfsys-pdftex.def}    % pdf
\def\pgfsysdriver@f{pgfsys-tex4ht.def}    % html
\def\pgfsysdriver@g{pgfsys-textures.def}  % ps
\def\pgfsysdriver@h{pgfsys-vtex.def}      % ps
\def\pgfsysdriver@i{pgfsys-xetex.def}     % pdf

\newif\ifgppdfout\gppdfoutfalse
\newif\ifgppsout\gppsoutfalse

\ifx\pgfsysdriver\pgfsysdriver@a
  \gppsouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@b
  \gppdfouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@c
  \gppdfouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@d
  \gppsouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@e
  \gppdfouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@f
  % tex4ht
\else\ifx\pgfsysdriver\pgfsysdriver@g
  \gppsouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@h
  \gppsouttrue
\else\ifx\pgfsysdriver\pgfsysdriver@i
  \gppdfouttrue
\fi\fi\fi\fi\fi\fi\fi\fi\fi

% uncomment the following lines to make font values "appendable"
% and if you are really sure about that ;-)
% \pgfkeyslet{/tikz/font/.@cmd}{\undefined}
% \tikzset{font/.initial={}}
% \def\tikz@textfont{\pgfkeysvalueof{/tikz/font}}

%
% image related stuff
%
\def\gp@rawimage@pdf#1#2#3#4#5#6{%
  \def\gp@tempa{cmyk}%
  \def\gp@tempb{#1}%
  \ifx\gp@tempa\gp@tempb%
    \def\gp@temp{/CMYK}%
  \else%
    \def\gp@temp{/RGB}%
  \fi%
  \pgf@sys@bp{#4}\pgfsysprotocol@literalbuffered{0 0}\pgf@sys@bp{#5}%
  \pgfsysprotocol@literalbuffered{0 0 cm}%
  \pgfsysprotocol@literalbuffered{BI /W #2 /H #3 /CS \gp@temp}%
  \pgfsysprotocol@literalbuffered{/BPC 8 /F /AHx ID}%
  \pgfsysprotocol@literal{#6 > EI}%
}
\def\gp@rawimage@ps#1#2#3#4#5#6{%
  \def\gp@tempa{cmyk}%
  \def\gp@tempb{#1}%
  \ifx\gp@tempa\gp@tempb%
    \def\gp@temp{4}%
  \else%
    \def\gp@temp{3}%
  \fi%
  \pgfsysprotocol@literalbuffered{0 0 translate}%
  \pgf@sys@bp{#4}\pgf@sys@bp{#5}\pgfsysprotocol@literalbuffered{scale}%
  \pgfsysprotocol@literalbuffered{#2 #3 8 [#2 0 0 -#3 0 #3]}%
  \pgfsysprotocol@literalbuffered{currentfile /ASCIIHexDecode filter}%
  \pgfsysprotocol@literalbuffered{false \gp@temp\space colorimage}%
  \pgfsysprotocol@literal{#6 >}%
}
\def\gp@rawimage@html#1#2#3#4#5#6{%
% FIXME: print a warning message here
}

\ifgppdfout
  \def\gp@rawimage{\gp@rawimage@pdf}
\else
  \ifgppsout
    \def\gp@rawimage{\gp@rawimage@ps}
  \else
    \def\gp@rawimage{\gp@rawimage@html}
  \fi
\fi


\def\gploadimage#1#2#3#4#5{%
  \pgftext[left,bottom,x=#1cm,y=#2cm] {\pgfimage[interpolate=false,width=#3cm,height=#4cm]{#5}};%
}

\def\gp@set@size#1{%
  \def\gp@image@size{#1}%
}

\def\gp@rawimage@#1#2#3#4#5#6#7#8{
  \tikz@scan@one@point\gp@set@size(#6,#7)\relax%
  \tikz@scan@one@point\pgftransformshift(#2,#3)\relax%
  \pgftext {%
    \pgfsys@beginpurepicture%
    \gp@image@size% fill \pgf@x and \pgf@y
    \gp@rawimage{#1}{#4}{#5}{\pgf@x}{\pgf@y}{#8}%
    \pgfsys@endpurepicture%
  }%
}

%% \gprawimage{color model}{xcoord}{ycoord}{# of xpixel}{# of ypixel}{xsize}{ysize}{rgb/cmyk hex data RRGGBB/CCMMYYKK ...}{file name}
%% color model is 'cmyk' or 'rgb' (default)
\def\gprawimage#1#2#3#4#5#6#7#8#9{%
  \ifx&#9&%
    \gp@rawimage@{#1}{#2}{#3}{#4}{#5}{#6}{#7}{#8}
  \else
    \ifgppsout
      \gp@rawimage@{#1}{#2}{#3}{#4}{#5}{#6}{#7}{#8}
    \else
      \gploadimage{#2}{#3}{#6}{#7}{#9}
    \fi
  \fi
}

%
% gnuplottex comapatibility
% (see http://www.ctan.org/tex-archive/help/Catalogue/entries/gnuplottex.html)
%

\def\gnuplottexextension@lua{\string tex}
\def\gnuplottexextension@tikz{\string tex}

%
% gnuplot variables getter and setter
%

\def\gpsetvar#1#2{%
  \expandafter\xdef\csname gp@var@#1\endcsname{#2}
}

\def\gpgetvar#1{%
  \csname gp@var@#1\endcsname %
}

%
% some wrapper code
%

% short for a filled path
\def\gpfill#1{\path[line width=0.1\gpbaselw,draw,fill,#1]}

% short for changing the line width
\def\gpsetlinewidth#1{\pgfsetlinewidth{#1\gpbaselw}}

% short for changing the line type
\def\gpsetlinetype#1{\tikzset{gp path/.style={#1,#1 add}}}

% short for changing the dash pattern
\def\gpsetdashtype#1{\tikzset{gp path/.append style={#1}}}

% short for changing the point size
\def\gpsetpointsize#1{\tikzset{gp point/.style={mark size=#1\gpbasems}}}

% wrapper for color settings
\def\gpcolor#1{\tikzset{global #1}}
\tikzset{rgb color/.code={\pgfutil@definecolor{.}{rgb}{#1}\tikzset{color=.}}}
\tikzset{global rgb color/.code={\pgfutil@definecolor{.}{rgb}{#1}\pgfutil@color{.}}}
\tikzset{global color/.code={\pgfutil@color{#1}}}

% prevent plot mark distortions due to changes in the PGF transformation matrix
% use `\gpscalepointstrue' and `\gpscalepointsfalse' for enabling and disabling
% point scaling
%
\newif\ifgpscalepoints
\tikzset{gp shift only/.style={%
  \ifgpscalepoints\else shift only\fi%
}}
\def\gppoint#1#2{%
  \path[solid] plot[only marks,gp point,mark options={gp shift only},#1] coordinates {#2};%
}


%
% char size calculation, that might be used with gnuplottex
%
% Example code (needs gnuplottex.sty):
%
%    % calculate the char size when the "gnuplot" style is used
%    \tikzset{gnuplot/.append style={execute at begin picture=\gpcalccharsize}}
%
%    \tikzset{gnuplot/.append style={font=\ttfamily\footnotesize}}
%
%    \begin{tikzpicture}[gnuplot]
%      \begin{gnuplot}[terminal=lua,%
%          terminaloptions={tikz solid nopic charsize \the\gphcharsize,\the\gpvcharsize}]
%        test
%      \end{gnuplot}
%    \end{tikzpicture}
%
%%%
% The `\gpcalccharsize' command fills the lengths \gpvcharsize and \gphcharsize with
% the values of the current default font used within nodes and is meant to be called
% within a tikzpicture environment.
% 
\newdimen\gpvcharsize
\newdimen\gphcharsize
\def\gpcalccharsize{%
  \pgfinterruptboundingbox%
  \pgfsys@begininvisible%
  \node at (0,0) {%
    \global\gphcharsize=1.05\fontcharwd\font`0%
    \global\gpvcharsize=1.05\fontcharht\font`0%
    \global\advance\gpvcharsize by 1.05\fontchardp\font`g%
  };%
  \pgfsys@endinvisible%
  \endpgfinterruptboundingbox%
}

%
%  define a rectangular node in tikz e.g. for the plot area
%
%  #1 node name
%  #2 coordinate of "south west"
%  #3 coordinate of "north east"
%
\def\gpdefrectangularnode#1#2#3{%
  \expandafter\gdef\csname pgf@sh@ns@#1\endcsname{rectangle}
  \expandafter\gdef\csname pgf@sh@np@#1\endcsname{%
    \def\southwest{#2}%
    \def\northeast{#3}%
  }
  \pgfgettransform\pgf@temp%
  % once it is defined, no more transformations will be applied, I hope
  \expandafter\xdef\csname pgf@sh@nt@#1\endcsname{\pgf@temp}%
  \expandafter\xdef\csname pgf@sh@pi@#1\endcsname{\pgfpictureid}%
}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%
%%  You may want to adapt the following to fit your needs (in your 
%%  individual style file and/or within your document).
%%

%
% style for every plot
%
\tikzset{gnuplot/.style={%
  >=stealth',%
  line cap=round,%
  line join=round,%
}}

\tikzset{gp node left/.style={anchor=mid west,yshift=-.12ex}}
\tikzset{gp node center/.style={anchor=mid,yshift=-.12ex}}
\tikzset{gp node right/.style={anchor=mid east,yshift=-.12ex}}

% basic plot mark size (points)
\newdimen\gpbasems
\gpbasems=.4pt

% basic linewidth
\newdimen\gpbaselw
\gpbaselw=.4pt

% this is the default color for pattern backgrounds
\colorlet{gpbgfillcolor}{white}

% set background color and fill color
\def\gpsetbgcolor#1{%
  \pgfutil@definecolor{gpbgfillcolor}{rgb}{#1}%
  \tikzset{tight background,background rectangle/.style={fill=gpbgfillcolor},show background rectangle}%
}

% this should reverse the normal text node presets, for the
% later referencing as described below
\tikzset{gp refnode/.style={coordinate,yshift=.12ex}}

% to add an empty label with the referenceable name "my node"
% to the plot, just add the following line to your gnuplot
% file:
%
% set label "" at 1,1 font ",gp refnode,name=my node"
%

% enlargement of the bounding box in standalone mode (only used by LaTeX/ConTeXt)
\def\gpbboxborder{0mm}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%
%%  The following TikZ-styles are derived from the 'pgf.styles.*' tables
%%  in the Lua script.
%%  To change the number of used styles you should change them there and
%%  regenerate this style file.
%%

]])
  f:write("% arrow styles settings\n")
  for i = 1, #pgf.styles.arrows do
    f:write("\\tikzset{"..pgf.styles.arrows[i][1].."/.style={"..pgf.styles.arrows[i][2].."}}\n")
  end
  f:write("\n% plotmark settings\n")
  for i = 1, #pgf.styles.plotmarks do
    f:write("\\tikzset{"..pgf.styles.plotmarks[i][1].."/.style={"..pgf.styles.plotmarks[i][2].."}}\n")
  end
  f:write("\n% pattern settings\n")
  for i = 1, #pgf.styles.patterns do
    f:write("\\tikzset{"..pgf.styles.patterns[i][1].."/.style={"..pgf.styles.patterns[i][2].."}}\n")
  end
  f:write("\n% if the 'tikzplot' option is used the corresponding lines will be smoothed by default\n")
  for i = 1, #pgf.styles.plotstyles_axes do
    f:write("\\tikzset{"..pgf.styles.plotstyles_axes[i][1].."/.style="..pgf.styles.plotstyles_axes[i][2].."}\n")
  end
  for i = 1, #pgf.styles.plotstyles do
    f:write("\\tikzset{"..pgf.styles.plotstyles[i][1].."/.style="..pgf.styles.plotstyles[i][2].."}\n")
  end
  -- line styles for borders etc ...
  f:write("\n% linestyle settings\n")
  for i = 1, #pgf.styles.linetypes_axes do
    f:write("\\tikzset{"..pgf.styles.linetypes_axes[i][1].."/.style="..pgf.styles.linetypes_axes[i][2].."}\n")
  end
  f:write("\n% linestyle \"addon\" settings for overwriting a default linestyle within the\n")
  f:write("% TeX document via eg. \\tikzset{gp lt plot 1 add/.style={fill=black,draw=none}} etc.\n")
  for i = 1, #pgf.styles.linetypes_axes do
    f:write("\\tikzset{"..pgf.styles.linetypes_axes[i][1].." add/.style={}}\n")
  end
  for i = 1, #pgf.styles.linetypes do
    f:write("\\tikzset{"..pgf.styles.linetypes[i][1].." add/.style={}}\n")
  end
  --[[ The line types. These don't do anything anymore, since we have the dash types. 
     But they were use to hook in from TikZ and change the respective styles 
     from the LaTeX script. We keep those definitions for backward compatibility. 
  ]]--
  for i = 1, #pgf.styles.linetypes do
    f:write("\\tikzset{"..pgf.styles.linetypes[i][1].."/.style={"..pgf.styles.linetypes[i][2].."}}\n")
  end
  f:write("\n% linestyle color settings\n")
  for i = 1, #pgf.styles.lt_colors_axes do
    f:write("\\colorlet{"..pgf.styles.lt_colors_axes[i][1].."}{"..pgf.styles.lt_colors_axes[i][2].."}\n")
  end
  -- dash styles for the plots
  f:write("\n% dash type settings\n")
  f:write("% Define this as a macro so that the dash patterns expand later with the current \\pgflinewidth.\n")
  f:write("\\def\\gpdashlength{\\pgflinewidth}\n")
  for i = 0, #pgf.styles.dashtypes do
    f:write("\\tikzset{"..pgf.styles.dashtypes[i][1].."/.style={"..pgf.styles.dashtypes[i][2].."}}\n")
  end
  for i = 1, #pgf.styles.dashtypes_axes do
    f:write("\\tikzset{"..pgf.styles.dashtypes_axes[i][1].."/.style={"..pgf.styles.dashtypes_axes[i][2].."}}\n")
  end

  f:write("\n% command for switching to colored lines\n")
  f:write("\\def\\gpcoloredlines{%\n")
  for i = 1, #pgf.styles.lt_colors do
    f:write("  \\colorlet{"..pgf.styles.lt_colors[i][1].."}{"..pgf.styles.lt_colors[i][2].."}%\n")
  end
  f:write("}\n")
  f:write("\n% command for switching to monochrome (black) lines\n")
  f:write("\\def\\gpmonochromelines{%\n")
  for i = 1, #pgf.styles.lt_colors do
    f:write("  \\colorlet{"..pgf.styles.lt_colors[i][1].."}{black}%\n")
  end
  f:write("}\n\n")
  f:write([[
%
% some initialisations
%
% by default all lines will be colored
\gpcoloredlines
\gpsetpointsize{4}
\gpsetlinetype{gp lt solid}
\gpscalepointsfalse
\endinput
]])
  f:close()
end


pgf.print_help = function(fwrite)

  fwrite([[
      {latex | tex | context}
      {color | monochrome}
      {nooriginreset | originreset}
      {nogparrows | gparrows}
      {nogppoints | gppoints}
      {picenvironment | nopicenvironment}
      {noclip | clip}
      {notightboundingbox | tightboundingbox}
      {background "<colorpec>"}
      {size <x>{unit},<y>{unit}}
      {scale <x>,<y>}
      {plotsize <x>{unit},<y>{unit}}
      {charsize <x>{unit},<y>{unit}}
      {font "<fontdesc>"}
      {{fontscale | textscale} <scale>}
      {dashlength | dl <DL>}
      {linewidth | lw <LW>}
      {nofulldoc | nostandalone | fulldoc | standalone}
      {{preamble | header} "<preamble_string>"}
      {tikzplot <ltn>,...}
      {notikzarrows | tikzarrows}
      {rgbimages | cmykimages}
      {noexternalimages|externalimages}
      {bitmap | nobitmap}
      {providevars <var name>,...}
      {createstyle}
      {help}

 For all options that expect lengths as their arguments they
 will default to 'cm' if no unit is specified. For all lengths
 the following units may be used: 'cm', 'mm', 'in' or 'inch',
 'pt', 'pc', 'bp', 'dd', 'cc'. Blanks between numbers and units
 are not allowed.

 'monochrome' disables line coloring and switches to grayscaled
 fills.

 'originreset' moves the origin of the TikZ picture to the lower
 left corner of the plot. It may be used to align several plots
 within one tikzpicture environment. This is not tested with
 multiplots and pm3d plots!

 'gparrows' use gnuplot's internal arrow drawing function
 instead of the ones provided by TikZ.

 'gppoints' use gnuplot's internal plotmark drawing function
 instead of the ones provided by TikZ.

 'nopicenvironment' omits the declaration of the 'tikzpicture'
 environment in order to set it manually. This permits putting
 some PGF/TikZ code directly before or after the plot.

 'clip' crops the plot at the defined canvas size. Default is
 'noclip' by which only a minimum bounding box of the canvas size
 is set. Neither a fixed bounding box nor a crop box is set if the
 'plotsize' or 'tightboundingbox' option is used.

 If 'tightboundingbox' is set the 'clip' option is ignored and the
 final bounding box is the natural bounding box calculated by tikz.

 'background' sets the background color to the value specified in
 the <colorpec> argument. <colorspec> must be a valid color name or
 a 3 byte RGB code as a hexadecimal number with a preceding number
 sign ('#'). E.g. '#ff0000' specifies pure red. If omitted the
 background is transparent.

 The 'size' option expects two lenghts <x> and <y> as the canvas
 size. The default size of the canvas is ]]..pgf.DEFAULT_CANVAS_SIZE_X..[[cm x ]]..pgf.DEFAULT_CANVAS_SIZE_Y..[[cm.

 The 'scale' option works similar to the 'size' option but expects
 scaling factors <x> and <y> instead of lengths.

 The 'plotsize' option permits setting the size of the plot area
 instead of the canvas size, which is the usual gnuplot behaviour.
 Using this option may lead to slightly asymmetric tic lengths.
 Like 'originreset' this option may not lead to convenient results
 if used with multiplots or pm3d plots. An alternative approach
 is to set all margins to zero and to use the 'noclip' option.
 The plot area has then the dimensions of the given canvas sizes.

 The 'charsize' option expects the average horizontal and vertical
 size of the used font. Look at the generated style file for an
 example of how to use it from within your TeX document.

 'fontscale' or 'textscale' expects a scaling factor as a parameter.
 All texts in the plot are scaled by this factor then.

 'dashlength' or 'dl' scales the length of dashed-line segments by <DL>,
 which is a floating-point number greater than zero. 'linewidth' or 
 'lw' scales all linewidths by <LW>.

 The options 'tex', 'latex' and 'context' choose the TeX output
 format. LaTeX is the default. To load the style file put the
 according line at the beginning of your document:
   \input ]]..pgf.STYLE_FILE_BASENAME..[[.tex    % (for plain TeX)
   \usepackage{]]..pgf.STYLE_FILE_BASENAME..[[}  % (for LaTeX)
   \usemodule[]]..pgf.STYLE_FILE_BASENAME..[[]   % (for ConTeXt)

 'createstyle' derives the TeX/LaTeX/ConTeXt styles from the script
 and writes them to the appropriate files.

 'fulldoc' or 'standalone' produces a full LaTeX document for direct
 compilation.

 'preamble' or 'header' may be used to put any additional LaTeX code
 into the document preamble in standalone mode.

 With the 'tikzplot' option the '\path plot' command will be used
 instead of only '\path'. The following list of numbers of linetypes
 (<ltn>,...) defines the affected plotlines. There exists a plotstyle
 for every linetype. The default plotstyle is 'smooth' for every
 linetype >= 1.

 By using the 'tikzarrows' option the gnuplot arrow styles defined by
 the user will be mapped to TikZ arrow styles. This is done by 'misusing'
 the angle value of the arrow definition. E.g. an arrow style with the
 angle '7' will be mapped to the TikZ style 'gp arrow 7' ignoring all the
 other given values. By default the TikZ terminal uses the stealth' arrow
 tips for all arrows. To obtain the default gnuplot behaviour please use
 the 'gparrows' option.

 With 'cmykimages' the CMYK color model will be used for inline image data
 instead of the RGB model. All other colors (like line colors etc.) are
 not affected by this option, since they are handled e.g. by LaTeX's
 xcolor package. This option is ignored if images are externalized.

 By using the 'externalimages' option all bitmap images will be written
 as external PNG images and included at compile time of the document.
 Generating DVI and later postscript files requires to convert the PNGs
 into EPS files in a seperate step e.g. by using ImageMagick's `convert`.
 Transparent bitmap images are always generated as an external PNGs.

 The 'nobitmap' option let images be rendered as filled rectangles instead
 of the nativ PS or PDF inline image format. This option is ignored if
 images are externalized.

 The 'providevars' options makes gnuplot's internal and user variables
 available by using the '\gpgetvar{<var name>}' commmand within the TeX
 script. Use gnuplot's 'show variables all' command to see the list
 of valid variables.

 The <fontdesc> string may contain any valid TeX/LaTeX/ConTeXt font commands
 like e.g. '\small'. It is passed directly as a node parameter in form of
 "font={<fontdesc>}". This can be 'misused' to add further code to a node,
 e.g. '\small,yshift=1ex' or ',yshift=1ex' are also valid while the
 latter does not change the current font settings. One exception is
 the second argument of the list. If it is a number of the form
 <number>{unit} it will be interpreted as a fontsize like in other
 terminals and will be appended to the first argument. If the unit is
 omitted the value is interpreted as 'pt'. As an example the string
 '\sffamily,12,fill=red' sets the font to LaTeX's sans serif font at
 a size of 12pt and red background color.
 The same applies to ConTeXt, e.g. '\switchtobodyfont[iwona],10' changes the
 font to Iwona at a size of 10pt.
 Plain TeX users have to change the font size explicitly within the first
 argument. The second should be set to the same value to get proper scaling
 of text boxes.

 Strings have to be put in single or double quotes. Double quoted
 strings may contain special characters like newlines '\n' etc.
]])
end


--[[===============================================================================================

  gfx.* helper functions
  
  Main intention is to prevent redundancies in the drawing
  operations and keep the pgf.* API as consistent as possible.
  
]]--===============================================================================================


-- path coordinates
gfx.path = {}
gfx.posx = nil
gfx.posy = nil

gfx.linetype_idx = nil       -- current linetype intended for the plot
gfx.linetype_idx_set = nil   -- current linetype set in the plot
gfx.dashtype_idx = nil       -- current dashtype intended for the plot
gfx.dashtype_idx_set = nil   -- current dashtype set in the plot
gfx.linewidth = nil
gfx.linewidth_set = nil
gfx.opacity = 1.0

gfx.boxed_text = false
gfx.boxed_text_count = 0    -- number of nodes inside the current box
gfx.boxed_text_xmargin = 0
gfx.boxed_text_ymargin = 0

-- internal calculated scaling factors
gfx.scalex = 1
gfx.scaley = 1

-- recalculate the origin of the plot
-- used for moving the origin to the lower left
-- corner...
gfx.origin_xoffset = 0
gfx.origin_yoffset = 0


-- color set in the document
gfx.color = ''
gfx.color_set = ''

gfx.pointsize = nil
gfx.pointsize_set = nil

gfx.text_font = ''
gfx.text_justify = "center"
gfx.text_angle = 0

-- option vars
gfx.opt = {
  latex_preamble = '',
  default_font = '',
  default_fontsize = 10,
  lines_colored = true,
  -- use gnuplot arrows or points instead of TikZ?
  gp_arrows = false,
  gp_points = false,
  -- don't put graphic commands into a tikzpicture environment
  nopicenv = false,
  -- produce full LaTeX document?
  full_doc = false,
  -- in gnuplot all sizes refer to the size of the canvas
  -- and not the size of plot itself
  plotsize_x = nil,
  plotsize_y = nil,
  set_plotsize = false,
  -- recalculate the origin of the plot
  -- used for moving the origin to the lower left
  -- corner...
  set_origin = false,
  -- list of _linetypes_ of plots that should be drawn as with the \plot
  -- command instead of \path
  plot_list = {},
  -- uses some pdf/ps specials with image function that will only work
  -- with pdf/ps generation!
  direct_image = true,
  -- list of gnuplot variables that should be made available via
  -- \gpsetvar{name}{val}
  gnuplot_vars = {},
  -- if true, the gnuplot arrow will be mapped to TikZ arrow styles by the
  -- given angle. E.g. an arrow with the angle `7' will be mapped to `gp arrow 7'
  -- style.
  tikzarrows = false,
  -- if true, cmyk image model will be used for bitmap images
  cmykimage = false,
  -- output TeX flavor, default is LaTeX
  tex_format = 'latex',
  -- for regression tests etc. we can turn off the timestamp
  notimestamp = false,
  -- background color, contains RGB triplet when set
  bgcolor = nil,
  -- crop to the given canvas size
  clip = false,
  -- if true, the natural bounding box will be used and 'clip' is ignored
  tightboundingbox = false,
  -- fontscale
  fontscale = nil,
  dashlength = nil,
  linewidth = 1.0
}

-- Formats for the various TeX flavors 
gfx.format = {}

gfx.format.tex = {
  docheader        = "\\input "..pgf.STYLE_FILE_BASENAME..".tex\n",
  begindocument    = "",
  enddocument      = "\\bye\n",
  beforetikzpicture= "",  -- standalone only
  aftertikzpicture = "",  -- standalone only
  begintikzpicture = "\\tikzpicture",
  endtikzpicture   = "\\endtikzpicture",
  beginscope       = "\\scope",
  endscope         = "\\endscope",
  beforeendtikzpicture = "",  -- standalone only
  fontsize         = ""
}

gfx.format.latex = {
  docheader        = "\\documentclass["..pgf.DEFAULT_FONT_SIZE.."pt]{article}\n"
                      .."\\usepackage[T1]{fontenc}\n"
                      .."\\usepackage{textcomp}\n\n"
                      .."\\usepackage[utf8x]{inputenc}\n\n"
                      .."\\usepackage{"..pgf.STYLE_FILE_BASENAME.."}\n"
                      .."\\pagestyle{empty}\n"
                      .."\\usepackage[active,tightpage]{preview}\n"
                      .."\\PreviewEnvironment{tikzpicture}\n"
                      .."\\setlength\\PreviewBorder{\\gpbboxborder}\n",
  begindocument    = "\\begin{document}\n",
  enddocument      = "\\end{document}\n",
  beforetikzpicture= "",  -- standalone only
  aftertikzpicture = "",  -- standalone only
  begintikzpicture = "\\begin{tikzpicture}",
  endtikzpicture   = "\\end{tikzpicture}",
  beginscope       = "\\begin{scope}",
  endscope         = "\\end{scope}",
  beforeendtikzpicture = "",  -- standalone only
  fontsize         = "\\fontsize{%spt}{%spt}\\selectfont"
}

gfx.format.context = {
  docheader        = "\\usemodule["..pgf.STYLE_FILE_BASENAME.."]\n",
  begindocument    = "\\starttext\n",
  enddocument      = "\\stoptext\n",
  beforetikzpicture= "\\startTEXpage\n",  -- standalone only
  aftertikzpicture = "\\stopTEXpage\n",   -- standalone only
  begintikzpicture = "\\starttikzpicture",
  endtikzpicture   = "\\stoptikzpicture",
  beginscope       = "\\startscope",
  endscope         = "\\stopscope",
  beforeendtikzpicture = "\\path[use as bounding box] ([shift={(-\\gpbboxborder,-\\gpbboxborder)}]current bounding box.south west)"
                            .." rectangle ([shift={(\\gpbboxborder,\\gpbboxborder)}]current bounding box.north east);\n",  -- standalone only
  fontsize         = "\\switchtobodyfont[%spt]"
}

-- within tikzpicture environment or not
gfx.in_picture = false

-- have not determined the plotbox, see the 'plotsize' option
gfx.have_plotbox = false

gfx.current_boundingbox = {
  xleft = nil, xright = nil, ytop = nil, ybot = nil
}
-- plot bounding boxes counter
gfx.boundingbox_cnt = 0

gfx.TEXT_ANCHOR = {
  ["left"]   = "gp node left",
  ["center"] = "gp node center",
  ["right"]  = "gp node right"
}

gfx.HEAD_STR = {"", "->", "<-", "<->"}


-- conversion factors in `cm'
gfx.units = {
  ['']    = 1,        -- default
  ['cm']  = 1,
  ['mm']  = 0.1,
  ['in']  = 2.54,
  ['inch']= 2.54,
  ['pt']  = 0.035146, -- Pica Point   (72.27pt = 1in)
  ['pc']  = 0.42176,  -- Pica         (1 Pica = 1/6 inch)
  ['bp']  = 0.035278, -- Big Point    (72bp = 1in)
  ['dd']  = 0.0376,   -- Didot Point  (1cm = 26.6dd)
  ['cc']  = 0.45113   -- Cicero       (1cc = 12 dd)
}


gfx.parse_number_unit = function (str, from, to)
  to = to or 'cm'
  from = from or 'cm'
  local num, unit = string.match(str, '^([%d%.]+)([a-z]*)$')
  if unit and (string.len(unit) > 0) then
    from = unit
  else
    unit = false
  end
  local factor_from = gfx.units[from]
  local factor_to   = gfx.units[to]
  num = tonumber(num)
  if num and factor_from then
    -- to cm and then to our target unit
    return num*(factor_from/factor_to), unit
  else
    return false, false
  end
end


gfx.parse_font_string = function (str)
  local size,rets,toks = nil, str, explode(',', str)
  -- set_font("") must restore default font
  if string.len(str) == 0 then
    return gfx.opt.default_font, gfx.opt.default_fontsize
  end
  -- if at least two tokens
  if #toks > 1 then
    -- add first element to font string
    rets = table.remove(toks,1)
    -- no unit means 'pt'
    size, _ = gfx.parse_number_unit(toks[1],'pt','pt')
    if (size) then
      table.remove(toks,1)
      rets = rets .. string.format(gfx.format[gfx.opt.tex_format].fontsize,size,size*1.2)
    end
    -- add grouping braces for the font settings
    if #rets > 0 then 
      rets = "{" .. rets .. "}"
    end
    -- add remaining parts
    for k,v in ipairs(toks) do
      rets = rets .. ',' .. v
    end
  else
    -- assume bare font name with no size
    if #rets > 0 then 
      rets = "{" .. rets .. "}"
    end
  end
  return rets, size
end


gfx.write_boundingbox = function()
  local t = gp.get_boundingbox()
  for k, v in pairs (t) do
    if v ~= gfx.current_boundingbox[k] then
      gfx.boundingbox_cnt = gfx.boundingbox_cnt + 1
      gfx.current_boundingbox = t
      pgf.write_boundingbox(t, gfx.boundingbox_cnt)
      break
    end  
  end
end

gfx.adjust_plotbox = function()
  local t = gp.get_boundingbox()
  if gfx.opt.set_origin then
    -- move origin to the lower left corner of the plot
    gfx.origin_xoffset = - t.xleft
    gfx.origin_yoffset = - t.ybot
  end
  if gfx.opt.set_plotsize then
    if (t.xright - t.xleft) > 0 then
      gfx.scalex = gfx.scalex*gfx.opt.plotsize_x * pgf.DEFAULT_RESOLUTION/(t.xright - t.xleft)
      gfx.scaley = gfx.scaley*gfx.opt.plotsize_y * pgf.DEFAULT_RESOLUTION/(t.ytop - t.ybot)
    else
      -- could not determin a valid bounding box, so keep using the
      -- plotsize as the canvas size
      gp.term_out("WARNING: PGF/TikZ Terminal: `plotsize' option used, but I could not determin the plot area!\n")
    end
  elseif not gfx.opt.tightboundingbox then
    if gfx.opt.clip then
      gp.write("\\clip")
    else
      gp.write("\\path")
    end
    gp.write(" (" .. pgf.format_coord(0,0) ..") rectangle (" .. pgf.format_coord(term.xmax, term.ymax) .. ");\n")
  end
end


gfx.check_variables = function()
  local vl = gfx.opt.gnuplot_vars
  local t = gp.get_all_variables()
  local sl = {}
  for i=1,#vl do
    if t[vl[i]] then
      sl[vl[i]] = t[vl[i]][3]
      if t[vl[i]][4] then
        sl[vl[i].." Im"] = t[vl[i]][4]
      end
    end
  end
  pgf.write_variables(sl)
end


-- check if the current path should be drawn
gfx.check_in_path = function()
  -- also check the bounding box here
  -- bounding box data is available with the first drawing command
  if (not gfx.have_plotbox) and gfx.in_picture then
    gfx.adjust_plotbox()
    gfx.have_plotbox = true
  end
  
  -- ignore zero length paths
  if #gfx.path > 1 then
    -- check all line properties and draw current path
    gfx.check_color()
    gfx.check_linetype()
    gfx.check_dashtype()
    gfx.check_linewidth()
    pgf.draw_path(gfx.path)
    -- remember last coordinates
    gfx.start_path(gfx.path[#gfx.path][1], gfx.path[#gfx.path][2])
  end
end

-- did the linetype change?
gfx.check_linetype = function()
  if gfx.linetype_idx ~= gfx.linetype_idx_set then
    local lt
    if gfx.linetype_idx == -1 or gfx.linetype_idx == -2 then
        lt = pgf.styles.linetypes_axes[math.abs(gfx.linetype_idx)][1]
    else
      lt = pgf.styles.linetypes[(gfx.linetype_idx % #pgf.styles.linetypes)+1][1]
    end
    pgf.set_linetype(lt)
    gfx.linetype_idx_set = gfx.linetype_idx
  end
end

-- did the dashtype change?
gfx.check_dashtype = function()
  if gfx.dashtype_idx ~= gfx.dashtype_idx_set then
    -- if gfx.dashtype_idx is a string, it contains a custom dash pattern
    if type(gfx.dashtype_idx) == type(1) then
      if gfx.dashtype_idx == -1 or gfx.dashtype_idx == -2 then
        pgf.set_dashtype(pgf.styles.dashtypes_axes[math.abs(gfx.dashtype_idx)][1])
      elseif gfx.dashtype_idx > 0 then
        pgf.set_dashtype(pgf.styles.dashtypes[(gfx.dashtype_idx % #pgf.styles.dashtypes) + 1][1])
      end
    else
       pgf.set_dashtype("dash pattern="..gfx.dashtype_idx)
    end
    gfx.dashtype_idx_set = gfx.dashtype_idx
  end
end

-- did the color change?
gfx.check_color = function()
  if gfx.color_set ~= gfx.color then
    pgf.set_color(gfx.color)
    gfx.color_set = gfx.color
  end
end

-- sanity check if we already are at this position in our path
-- and save this position
gfx.check_coord = function(x, y)
  if (x == gfx.posx) and (y == gfx.posy) then
    return true
  end
  gfx.posx = x
  gfx.posy = y
  return false
end

-- did the linewidth change?
gfx.check_linewidth = function()
  if gfx.linewidth ~= gfx.linewidth_set then
    pgf.set_linewidth(gfx.linewidth)
    gfx.linewidth_set = gfx.linewidth
  end
end

-- did the pointsize change?
gfx.check_pointsize = function()
  if gfx.pointsize ~= gfx.pointsize_set then
    pgf.set_pointsize(gfx.pointsize)
    gfx.pointsize_set = gfx.pointsize
  end
end


gfx.start_path = function(x, y)
  --  init path with first coords
  gfx.path = {{x,y}}  
  gfx.posx = x
  gfx.posy = y
end

-- ctype  string  LT|RGBA|GRAY
-- val   table   {name}|{r,g,b,alpha}
-- returns a properly formatted color parameter string, or nil if LT_NODRAW was used.
gfx.format_color = function(ctype, val)
  local c
  if ctype == 'LT' then
    if val[1] < 0 then
      if val[1] == -3 then
        c = nil
      elseif val[1] < -2 then --  LT_NODRAW, LT_BACKGROUND, LT_UNDEFINED
        c = 'color=gpbgfillcolor'
      else
        c = 'color='..pgf.styles.lt_colors_axes[math.abs(val[1])][1]
      end
    else
      c = 'color='..pgf.styles.lt_colors[(val[1] % #pgf.styles.lt_colors)+1][1]
    end
    -- c = pgf.styles.lt_colors[((val[1]+3) % #pgf.styles.lt_colors) + 1][1]
  elseif ctype == 'RGBA' then
    c = string.format("rgb color={%.3f,%.3f,%.3f}", val[1], val[2], val[3])
  elseif ctype == 'GRAY' then
    c = string.format("color=black!%i", 100*val[1]+0.5)
  end
  return c
end

gfx.set_opacity = function(ctype, val)
  gfx.opacity = 1.0
  if (ctype == 'RGBA') then
     gfx.opacity = val[4]
  end
end

gfx.set_color = function(ctype, val)
  gfx.color = gfx.format_color(ctype, val)
  gfx.set_opacity(ctype, val)
end


--[[===============================================================================================

  The terminal layer
  
  The term.* functions are usually called from the gnuplot Lua terminal

]]--===============================================================================================


if arg then
  -- when called from the command line we have
  -- to initialize the table `term' manually
  -- to avoid errors
  term = {}
else
  --
  -- gnuplot terminal default parameters and flags
  --
  term.xmax = pgf.DEFAULT_RESOLUTION * pgf.DEFAULT_CANVAS_SIZE_X
  term.ymax = pgf.DEFAULT_RESOLUTION * pgf.DEFAULT_CANVAS_SIZE_Y
  term.h_tic =  pgf.DEFAULT_RESOLUTION * pgf.DEFAULT_TIC_SIZE
  term.v_tic =  pgf.DEFAULT_RESOLUTION * pgf.DEFAULT_TIC_SIZE
  -- default size for CM@10pt
  term.h_char = math.floor(pgf.DEFAULT_FONT_H_CHAR * (pgf.DEFAULT_FONT_SIZE/10) * (pgf.DEFAULT_RESOLUTION/1000) + .5)
  term.v_char = math.floor(pgf.DEFAULT_FONT_V_CHAR * (pgf.DEFAULT_FONT_SIZE/10) * (pgf.DEFAULT_RESOLUTION/1000) + .5)
  term.description = "Lua PGF/TikZ terminal for TeX and friends"
  term_default_flags = term.TERM_BINARY + term.TERM_CAN_MULTIPLOT + term.TERM_CAN_DASH + term.TERM_ALPHA_CHANNEL
                       + term.TERM_LINEWIDTH + term.TERM_IS_LATEX + term.TERM_FONTSCALE
  term.flags = term_default_flags + term.TERM_CAN_CLIP
end


--
-- initial = 1  for the initial "set term" call
--           0  for subsequent option changes -- currently unused, since the changeable options
--              are hardcoded within gnuplot :-(
--
-- t_count   see e.g. int_error()
--
term.options = function(opt_str, initial, t_count)

  local o_next = ""
  local o_type = nil
  local s_start, s_end = 1, 1
  local term_opt = ""
  local term_opt_font, term_opt_size, term_opt_background, term_opt_fontscale, term_opt_dashlength, term_opt_linewidth, term_opt_scale, term_opt_preamble = "", "", "", "", "", "", "", ""
  local charsize_h, charsize_v, fontsize, fontscale, dashlength = nil, nil, nil, nil, nil
  -- trim spaces
  opt_str = opt_str:gsub("^%s*(.-)%s*$", "%1")
  local opt_len = string.len(opt_str)

  t_count = t_count - 1

  local set_t_count = function(num) 
    -- gnuplot handles commas as regular tokens
    t_count = t_count + 2*num - 2
  end

  local almost_equals = function(param, opt)
    local op1, op2

    local st, _ = string.find(opt, "$", 2, true)
    if st then
      op1 = string.sub(opt, 1, st-1)
      op2 = string.sub(opt, st+1)
      if (string.sub(param, 1, st-1) == op1)
          and (string.find(op1..op2, param, 1, true) == 1) then
        return true
      end
    elseif opt == param then
      return true
    end
    return false
  end

  --
  -- simple parser for options and strings
  --
  local get_next_token = function()

    -- beyond the limit?
    if s_start > opt_len then
      o_next = ""
      o_type = nil
      return
    end

    t_count = t_count + 1

    -- search the start of the next token
    s_start, _ = string.find (opt_str, '[^%s]', s_start)
    if not s_start then
      o_next = ""
      o_type = nil
      return
    end

    -- a new string argument?
    local next_char = string.sub(opt_str, s_start, s_start)
    if next_char == '"' or next_char == "'" then
      -- find the end of the string by searching for
      -- the next not escaped quote
      _ , s_end = string.find (opt_str, '[^\\]'..next_char, s_start+1)
      if s_end then
        o_next = string.sub(opt_str, s_start+1, s_end-1)
        if next_char == '"' then
          -- Wow! this is to resolve all string escapes, kind of "unescape string"
          o_next = assert(loadstring("return(\""..o_next.."\")"))()
        end
        o_type = "string"
      else
        -- FIXME: error: string does not end...
        -- seems that gnuplot adds missing quotes
        -- so this will never happen...
      end
    else
      -- ok, it's not a string...
      -- then find the next white space or end of line
      -- comma separated strings are regarded as one token
      s_end, _ = string.find (opt_str, '[^,][%s]+[^,]', s_start)
      if not s_end then -- reached the end of the string
        s_end = opt_len + 1
      else
        s_end = s_end + 1
      end
      o_next = string.sub(opt_str, s_start, s_end-1)
      if tonumber(o_next) ~= nil then
        o_type = 'number'
      else
      o_type = "op"
    end
    end
    s_start = s_end + 1
    return
  end    

  local get_two_sizes = function(str)
    local args = explode(',', str)
    set_t_count(#args)

    local num1, num2, unit
    if #args ~= 2 then
      return false, nil
    else
      num1, unit = gfx.parse_number_unit(args[1])
      if unit then
        t_count = t_count + 1
      end
      num2, unit = gfx.parse_number_unit(args[2])
      if unit then
        t_count = t_count + 1
      end
      if not (num1 and num2) then
        return false, nil
      end
    end
    return num1, num2
  end
  
  local print_help = false

  while true do
    get_next_token()
    if not o_type then break end
    if almost_equals(o_next, "he$lp") then
      print_help = true
    elseif almost_equals(o_next, "mono$chrome") then
      -- no colored lines
      -- Setting `term.TERM_MONOCHROME' would internally disable colors for all drawings.
      -- We do it the `soft' way by redefining all colors via a TeX command.
      -- Maybe an additional terminal option is useful here...
      gfx.opt.lines_colored = false
    elseif almost_equals(o_next, "c$olor") or almost_equals(o_next, "c$olour") then
      -- colored lines
      gfx.opt.lines_colored = true
    elseif almost_equals(o_next, "notime$stamp") then
      -- omit output of the timestamp
      gfx.opt.notimestamp = true
    elseif almost_equals(o_next, "gparr$ows") then
      -- use gnuplot arrows instead of TikZ
      gfx.opt.gp_arrows = true
    elseif almost_equals(o_next, "nogparr$ows") then
      -- use gnuplot arrows instead of TikZ
      gfx.opt.gp_arrows = false
    elseif almost_equals(o_next, "gppoint$s") then
      -- use gnuplot points instead of TikZ
      gfx.opt.gp_points = true
    elseif almost_equals(o_next, "nogppoint$s") then
      -- use gnuplot points instead of TikZ
      gfx.opt.gp_points = false
    elseif almost_equals(o_next, "nopic$environment") then
      -- omit the 'tikzpicture' environment
      gfx.opt.nopicenv = true
    elseif almost_equals(o_next, "pic$environment") then
      -- omit the 'tikzpicture' environment
      gfx.opt.nopicenv = false
    elseif almost_equals(o_next, "origin$reset") then
      -- moves the origin of the TikZ picture to the lower left corner of the plot
      gfx.opt.set_origin = true
    elseif almost_equals(o_next, "noorigin$reset") then
      -- moves the origin of the TikZ picture to the lower left corner of the plot
      gfx.opt.set_origin = false
    elseif almost_equals(o_next, "plot$size") then
      get_next_token()
      gfx.opt.plotsize_x, gfx.opt.plotsize_y = get_two_sizes(o_next)
      if not gfx.opt.plotsize_x then
        gp.int_error(t_count, string.format("error: two comma seperated lengths expected, got `%s'.", o_next))
      end
      gfx.opt.set_plotsize = true
      term_opt_size = string.format("plotsize %s,%s ", gfx.opt.plotsize_x, gfx.opt.plotsize_y)
      -- we set the canvas size to the plotsize to keep the aspect ratio as good as possible
      -- and rescale later once we know the actual plot size...
      term.xmax = gfx.opt.plotsize_x*pgf.DEFAULT_RESOLUTION
      term.ymax = gfx.opt.plotsize_y*pgf.DEFAULT_RESOLUTION
    elseif almost_equals(o_next, "si$ze") then
      get_next_token()
      local plotsize_x, plotsize_y = get_two_sizes(o_next)
      if not plotsize_x then
        gp.int_error(t_count, string.format("error: two comma seperated lengths expected, got `%s'.", o_next))
      end
      gfx.opt.set_plotsize = false
      term_opt_size = string.format("size %s,%s ", plotsize_x, plotsize_y)
      term.xmax = plotsize_x*pgf.DEFAULT_RESOLUTION
      term.ymax = plotsize_y*pgf.DEFAULT_RESOLUTION
    elseif almost_equals(o_next, "char$size") then
      get_next_token()
      charsize_h, charsize_v = get_two_sizes(o_next)
      if not charsize_h then
        gp.int_error(t_count, string.format("error: two comma seperated lengths expected, got `%s'.", o_next))
      end
    elseif almost_equals(o_next, "sc$ale") then
      get_next_token()
      local xscale, yscale = get_two_sizes(o_next)
      if not xscale then
        gp.int_error(t_count, string.format("error: two comma seperated numbers expected, got `%s'.", o_next))
      end
      term_opt_scale = string.format("scale %s,%s ",xscale, yscale)
      term.xmax = term.xmax * xscale
      term.ymax = term.ymax * yscale
    elseif almost_equals(o_next, "tikzpl$ot") then
      get_next_token()
      local args = explode(',', o_next)
      set_t_count(#args)
      for i = 1,#args do
        args[i] = tonumber(args[i])
        if args[i] == nil then
          gp.int_error(t_count, string.format("error: list of comma seperated numbers expected, got `%s'.", o_next))
        end
        args[i] = args[i] - 1
      end
      gfx.opt.plot_list = args
    elseif almost_equals(o_next, "provide$vars") then
      get_next_token()
      local args = explode(',', o_next)
      set_t_count(#args)
      gfx.opt.gnuplot_vars = args
    elseif almost_equals(o_next, "tikzar$rows") then
      -- map the arrow angles to TikZ arrow styles
      gfx.opt.tikzarrows = true
    elseif almost_equals(o_next, "notikzar$rows") then
      -- don't map the arrow angles to TikZ arrow styles
      gfx.opt.tikzarrows = false
    elseif almost_equals(o_next, "nobit$map") then
      -- render images as filled rectangles instead of the nativ
      -- PS or PDF image format
      gfx.opt.direct_image = false
    elseif almost_equals(o_next, "bit$map") then
      -- render images as nativ PS or PDF image
      gfx.opt.direct_image = true
    elseif almost_equals(o_next, "cmyk$image") then
      -- use cmyk color model for images
      gfx.opt.cmykimage = true
    elseif almost_equals(o_next, "rgb$image") then
      -- use cmyk color model for images
      gfx.opt.cmykimage = false
    elseif almost_equals(o_next, "full$doc") or almost_equals(o_next, "stand$alone") then
      -- produce full tex document
      gfx.opt.full_doc = true
    elseif almost_equals(o_next, "nofull$doc") or almost_equals(o_next, "nostand$alone") then
      -- produce full tex document
      gfx.opt.full_doc = true
    elseif almost_equals(o_next, "create$style") then
      -- creates the coresponding LaTeX style from the script
      pgf.create_style()
    elseif almost_equals(o_next, "backg$round") then
      -- set background color
      get_next_token()
      -- ignore rgbcolor keyword if present
      if almost_equals(o_next, "rgb$color") then get_next_token() end
      if o_type == 'string' then
        gfx.opt.bgcolor = gp.parse_color_name(t_count, o_next)
        term_opt_background = string.format("background '%s'", o_next)
      else
        gp.int_error(t_count, string.format("error: string expected, got `%s'.", o_next))
      end
    elseif almost_equals(o_next, "fo$nt") then
      get_next_token()
      if o_type == 'string' then
        gfx.opt.default_font, fontsize = gfx.parse_font_string(o_next)
      else
        gp.int_error(t_count, string.format("error: string expected, got `%s'.", o_next))
      end
      term_opt_font = string.format("font %q ", o_next)
    elseif almost_equals(o_next, "fonts$cale") or  almost_equals(o_next, "texts$cale") then
      get_next_token()
      if o_type == 'number' then
        fontscale = tonumber(o_next)
        if fontscale < 0 then
          fontscale = 1.0
        end
      else
        gp.int_error(t_count, string.format("error: number expected, got `%s'.", o_next))
      end
    elseif almost_equals(o_next, "dashl$ength") or almost_equals(o_next, "dl") then
      get_next_token()
      if o_type == 'number' then
        dashlength = tonumber(o_next)
        if dashlength <= 0 then
	  dashlength = 1.0
	end
      else
        gp.int_error(t_count, string.format("error: number expected, got `%s'.", o_next))
      end
    elseif almost_equals(o_next, "linew$idth") or almost_equals(o_next, "lw") then
      get_next_token()
      if o_type == 'number' then
        gfx.opt.linewidth = tonumber(o_next)
        if gfx.opt.linewidth <= 0 then
          gfx.opt.linewidth = 1.0
        end
	term_opt_linewidth = string.format("linewidth %.1f ", gfx.opt.linewidth)
      else
        gp.int_error(t_count, string.format("error: number expected, got `%s'.", o_next))
      end
    elseif almost_equals(o_next, "externalimages") then
      if term.external_images ~= nil then
        term.external_images = true;
      else
        gp.int_warn(t_count, "Externalization of images is not supported.")
      end
    elseif almost_equals(o_next, "noexternalimages") then
      if term.external_images ~= nil then
        term.external_images = false;
      else
        gp.int_warn(t_count, "Externalization of images is not supported.")
      end
    elseif almost_equals(o_next, "pre$amble") or almost_equals(o_next, "header") then
      get_next_token()
      if o_type == 'string' then
        term_opt_preamble = term_opt_preamble .. string.format("preamble %q ", o_next)
        gfx.opt.latex_preamble = gfx.opt.latex_preamble .. o_next .. "\n"
      else
        gp.int_error(t_count, string.format("error: string expected, got `%s'.", o_next))
      end
    elseif almost_equals(o_next, "nopre$amble") or almost_equals(o_next, "noheader") then
        gfx.opt.latex_preamble = ''
    elseif almost_equals(o_next, "con$text") then
      gfx.opt.tex_format = "context"
      -- ConTeXt has a default of 12pt
      fontsize = 12
    elseif almost_equals(o_next, "tex") then
      gfx.opt.tex_format = "tex"
    elseif almost_equals(o_next, "latex") then
      gfx.opt.tex_format = "latex"
    elseif almost_equals(o_next, "clip") then
      gfx.opt.clip = true
      term.flags = term_default_flags
      term.flags = term_default_flags + term.TERM_CAN_CLIP
    elseif almost_equals(o_next, "noclip") then
      gfx.opt.clip = false
      term.flags = term_default_flags
    elseif almost_equals(o_next, "tight$boundingbox") then
      gfx.opt.tightboundingbox = true
    elseif almost_equals(o_next, "notight$boundingbox") then
      gfx.opt.tightboundingbox = false
    else
      gp.int_warn(t_count, string.format("unknown option `%s'.", o_next))
    end
  end

  -- determine "internal" font size
  -- FIXME: what happens on "set termoptions font ..." or subsequent terminal calls
  local term_h_char, term_v_char = term.h_char, term.v_char
  if fontsize ~= nil then
    gfx.opt.default_fontsize = fontsize
    term_h_char = pgf.DEFAULT_FONT_H_CHAR * (fontsize/10) * (pgf.DEFAULT_RESOLUTION/1000)
    term_v_char = pgf.DEFAULT_FONT_V_CHAR * (fontsize/10) * (pgf.DEFAULT_RESOLUTION/1000)
    -- on change apply old text scaling
    if not fontscale then
      fontscale = gfx.opt.fontscale;
    end
  end
  -- a given character size overwrites font size
  if charsize_h ~= nil then
    term_h_char = charsize_h*pgf.DEFAULT_RESOLUTION
    term_v_char = charsize_v*pgf.DEFAULT_RESOLUTION
    -- on change apply old text scaling
    if not fontscale then
      fontscale = gfx.opt.fontscale;
    end
  end
  if fontscale ~= nil then
    term_h_char = term_h_char * fontscale
    term_v_char = term_v_char * fontscale
    gfx.opt.fontscale = fontscale;
    term_opt_fontscale = string.format("fontscale %s", gfx.opt.fontscale)
  end
  term.h_char = math.floor(term_h_char + .5)
  term.v_char = math.floor(term_v_char + .5)

  if dashlength ~= nil then
    gfx.opt.dashlength = dashlength
    term_opt_dashlength = string.format("dashlength %.1f", dashlength)
  end 

  if print_help then
    pgf.print_help(gp.term_out)
  end

  local tf = function(b,y,n)
    local addopt = ''
    if b then 
      addopt = y
    else
      addopt = n
    end
    if (string.len(addopt) > 0) then
      term_opt = term_opt .. addopt .. ' '
    end
  end

  tf(true, gfx.opt.tex_format, nil)
  tf(true, term_opt_font, nil)
  tf(true, term_opt_size, nil)
  tf(true, term_opt_background, nil)
  tf(true, term_opt_fontscale, nil)
  tf(true, term_opt_dashlength, nil)
  tf(true, term_opt_linewidth, nil)
  tf((#gfx.opt.latex_preamble>0), term_opt_preamble, 'nopreamble')
  tf(gfx.opt.lines_colored, 'color', 'monochrome')
  tf(gfx.opt.full_doc, 'standalone', 'nostandalone')
  tf(gfx.opt.gp_arrows, 'gparrows', 'nogparrows')
  tf(gfx.opt.tikzarrows, 'tikzarrows', 'notikzarrows')
  tf(gfx.opt.gp_points, 'gppoints', 'nogppoints')
  tf(gfx.opt.nopicenv, 'nopicenvironment', 'picenvironment')
  tf(gfx.opt.set_origin, 'originreset', 'nooriginreset')
  tf(gfx.opt.direct_image, 'bitmap', 'nobitmap')
  tf(gfx.opt.cmykimage, 'cmykimage', 'rgbimage')
  tf(gfx.opt.clip, 'clip', 'noclip')
  tf(gfx.opt.tightboundingbox, 'tightboundingbox', 'notightboundingbox')
  if term.external_images ~= nil then
    tf(term.external_images, 'externalimages', 'noexternalimages')
  end
  gp.term_options(term_opt)

  return 1
end

-- Called once, when the device is first selected.
term.init = function()
  if gfx.opt.full_doc then
    pgf.write_doc_begin(gfx.opt.latex_preamble)
  end
  return 1
end

-- Called just before a plot is going to be displayed.
term.graphics = function()
  -- reset some state variables
  gfx.linetype_idx_set = nil
  gfx.dashtype_idx_set = nil
  gfx.linewidth_set = nil
  gfx.pointsize_set = nil
  gfx.color_set = nil
  gfx.in_picture = true
  gfx.have_plotbox = false
  gfx.boundingbox_cnt = 0
  gfx.scalex = 1/pgf.DEFAULT_RESOLUTION
  gfx.scaley = 1/pgf.DEFAULT_RESOLUTION
  gfx.current_boundingbox = {
    xleft = nil, xright = nil, ytop = nil, ybot = nil
  }

    -- put a newline between subsequent plots in fulldoc mode...
  if gfx.opt.full_doc then
    gp.write("\n")
  end
  pgf.write_graph_begin(gfx.opt.default_font, gfx.opt.nopicenv)
  return 1
end


term.vector = function(x, y)
  if gfx.linetype_idx ~= -3 then
    if #gfx.path == 0 then
      gfx.start_path(gfx.posx, gfx.posy)
    elseif not gfx.check_coord(x, y) then
      -- checked for zero path length and add the path coords to gfx.path
      gfx.path[#gfx.path+1] = {x,y}
    end
  end
  return 1
end

term.move = function(x, y)
  if gfx.linetype_idx ~= -3 then
    -- only "move" if we change our latest position
    if not gfx.check_coord(x, y) then
      -- finish old path and start a new one
      gfx.check_in_path()
      gfx.start_path(x, y)
    end
  end
  return 1
end

term.linetype = function(ltype)
  gfx.check_in_path()

  gfx.set_color('LT', {ltype})

  if (ltype < -4) then -- LT_NODRAW = -3, LT_BACKGROUND = -4
    ltype = -3
  end

  if ltype == -1 then  -- LT_AXIS
    gfx.dashtype_idx = -2
  end
  if ltype == -2 then  -- LT_SOLID
    gfx.dashtype_idx = -1
  end
  gfx.linetype_idx = ltype

  return 1
end

term.dashtype = function(dtype, pattern)
   gfx.check_in_path()
   
   if dtype == -3 then -- DASHTYPE_CUSTOM
       gfx.dashtype_idx = ''

       for i = 1,#pattern do
  	  if (i % 2) == 1 then
              gfx.dashtype_idx = gfx.dashtype_idx .. 'on '
          else
	      gfx.dashtype_idx = gfx.dashtype_idx .. 'off '
          end
          gfx.dashtype_idx = gfx.dashtype_idx..string.format('%.2f*\\gpdashlength ', pattern[i])
       end
   else
       gfx.dashtype_idx = dtype
   end

   return 1
end


term.point = function(x, y, num)
  if gfx.opt.gp_points then
    return 0
  else
    gfx.check_in_path()
    gfx.check_color()
    gfx.check_linewidth()
    gfx.check_pointsize()
  
    local pm
    if num == -1 then
      pm = pgf.styles.plotmarks[1][1]
    else
      pm = pgf.styles.plotmarks[(num % (#pgf.styles.plotmarks-1)) + 2][1]
    end
    pgf.draw_points({{x,y}}, pm)
    
    return 1
  end
end


--[[
  this differs from the original API
  one may use the additional parameters to define own styles
  e.g. "misuse" angle for numbering predefined styles...

  int length        /* head length */
  double angle      /* head angle in degrees */
  double backangle  /* head back angle in degrees */
  int filled        /* arrow head filled or not */
]]
term.arrow = function(sx, sy, ex, ey, head, length, angle, backangle, filled)
  if gfx.linetype_idx == -3 then -- LT_NODRAW
    return 1
  end
  if gfx.opt.gp_arrows then
    return 0
  else
    local headstyle = 0
    if gfx.opt.tikzarrows then
      headstyle = angle
    end
    gfx.check_in_path()
    gfx.check_color()
    gfx.check_linetype()
    gfx.check_dashtype()
    gfx.check_linewidth()
    pgf.draw_arrow({{sx,sy},{ex,ey}}, gfx.HEAD_STR[head+1], headstyle)
    return 1
  end
end

-- Called immediately after a plot is displayed.
term.text = function()
  gfx.check_in_path()
  pgf.write_graph_end(gfx.opt.nopicenv)
  gfx.in_picture = false
  return 1
end

term.put_text = function(x, y, txt)
  gfx.check_in_path()
  gfx.check_color()
  
  if (txt ~= '') or (gfx.text_font ~= '')  then -- omit empty nodes
    pgf.write_text_node({x, y}, txt, gfx.text_angle, gfx.TEXT_ANCHOR[gfx.text_justify], gfx.text_font)
  end
  return 1
end

term.justify_text = function(justify)
  gfx.text_justify = justify
  return 1
end

term.text_angle = function(ang)
  gfx.text_angle = ang
  return 1
end

term.boxed_text = function(x, y, option)
   if (option == 'INIT') then
      gfx.boxed_text = true
      gfx.boxed_text_count = 0
   elseif (option == 'MARGINS') then
      gfx.boxed_text_xmargin = x / 100.0
      gfx.boxed_text_ymargin = y / 100.0
   elseif (option == 'BACKGROUNDFILL' or option == 'OUTLINE') then
      gfx.check_color()
      gfx.check_linetype()
      gfx.check_dashtype()
      gfx.check_linewidth()
      if (gfx.boxed_text_count > 0) then
	 gp.write('\\node[')
	 if (option == 'BACKGROUNDFILL') then
	    gp.write('fill = gpbgfillcolor,')
	 else
	    gfx.boxed_text = false
	    gp.write('draw, gp path,')
	 end
	 gp.write(string.format('inner xsep=%.2f, inner ysep=%.2f,', gfx.boxed_text_xmargin, gfx.boxed_text_ymargin))
	 gp.write('fit=')
	 for i=1,gfx.boxed_text_count do
	    gp.write(string.format('(gp boxed node %d)', i))
	 end
	 gp.write(']{};\n')
      end
   elseif (option == 'FINISH') then
      gfx.boxed_text = false
   end
   return 1
end

term.linewidth = function(width)
  width = width * gfx.opt.linewidth
  if gfx.linewidth ~= width then
    gfx.check_in_path()
    gfx.linewidth = width
  end
  return 1
end

term.pointsize = function(size)
  if gfx.pointsize ~= size then
    gfx.check_in_path()
    gfx.pointsize = size
  end
  return 1
end

term.set_font = function(font)
  local fontsize = nil
  gfx.text_font, fontsize = gfx.parse_font_string(font)
  if fontsize then
    term.h_char = math.floor(pgf.DEFAULT_FONT_H_CHAR * (fontsize/10) * (pgf.DEFAULT_RESOLUTION/1000) + 0.5)
    term.v_char = math.floor(pgf.DEFAULT_FONT_V_CHAR * (fontsize/10) * (pgf.DEFAULT_RESOLUTION/1000) + 0.5)
  end
  return 1
end

-- at the moment this is only used to check
-- the plot's bounding box as seldom as possible
term.layer = function(l)
  if l == 'end_text' then
    -- called after a plot is finished (also after each "mutiplot")
    gfx.write_boundingbox()
  end
  return 1
end

-- we don't use this, because we are implicitly testing
-- for closed paths
term.path = function(p)
  return 1
end


term.filled_polygon = function(style, fillpar, t)
  local pattern = nil
  local color = nil
  local opacity = 100
  local saturation = 100
  
  gfx.check_in_path()

  if style == 'EMPTY' then
      -- FIXME: should be the "background color" and not gpbgfillcolor
      pattern = ''
      color = 'gpbgfillcolor'
      saturation = 100
      opacity = 100
  elseif style == 'DEFAULT' or style == 'OPAQUE' then -- FIXME: not shure about the opaque style
      pattern = ''
      color = gfx.color
      saturation = 100
      opacity = 100 * gfx.opacity
  elseif style == 'SOLID' then
      pattern = ''
      color = gfx.color
      if fillpar < 100 then
        saturation = fillpar
      else
        saturation = 100
      end
      opacity = 100
  elseif style == 'PATTERN' then
      pattern = pgf.styles.patterns[(fillpar % #pgf.styles.patterns) + 1][1]
      color = gfx.color
      saturation = 100
      opacity = 100
  elseif style == 'TRANSPARENT_SOLID' then
      pattern = ''
      color = gfx.color
      saturation = 100
      opacity = fillpar
  elseif style == 'TRANSPARENT_PATTERN' then
      pattern = pgf.styles.patterns[(fillpar % #pgf.styles.patterns) + 1][1]
      color = gfx.color
      saturation = 100
      opacity = 0
  end
  
  pgf.draw_fill(t, pattern, color, saturation, opacity)  
  
  return 1
end


term.boxfill = function(style, fillpar, x1, y1, width, height)
  local t = {{x1, y1}, {x1+width, y1}, {x1+width, y1+height}, {x1, y1+height}}
  return term.filled_polygon(style, fillpar, t)
end

-- points[row][column]
-- m: #cols, n: #rows
-- corners: clip box and draw box coordinates
-- ctype: "RGBA" or "PALETTE"
term.image = function(m, n, points, corners, ctype, xfile)
  gfx.check_in_path()
  
  pgf.write_clipbox_begin({corners[3][1],corners[3][2]},{corners[4][1],corners[4][2]})
  
    local ll = {corners[1][1],corners[2][2]}
    local ur = {corners[2][1],corners[1][2]}
  local xxfile

  if xfile ~= nil then
    -- strip file extension
    xxfile = string.match(xfile, "^(.*).png$")
  else
    xxfile = ""
  end
  -- load exclusively an external file and don't generate inline images
  if xfile ~= nil and term.external_images == true then
     pgf.load_image_file(ll, ur, xxfile)
  elseif gfx.opt.direct_image then
    if gfx.opt.cmykimage then
      pgf.draw_raw_cmyk_image(points, m, n, ll, ur, xxfile)
    else
      pgf.draw_raw_rgb_image(points, m, n, ll, ur, xxfile)
    end
  else
    -- draw as filled squares
    local w = (corners[2][1] - corners[1][1])/m
    local h = (corners[1][2] - corners[2][2])/n

    local yy,yyy,xx,xxx,color
    for cnt = 1,#points do
      xx = corners[1][1]+(cnt%m-1)*w
      yy = corners[1][2]-math.floor(cnt/m)*h
      yyy = yy-h
      xxx = xx+w
      color = gfx.format_color(ctype, points[cnt])
      gfx.set_opacity(ctype, points[cnt])
      if color ~= nil then
        pgf.draw_fill({{xx, yy}, {xxx, yy}, {xxx, yyy}, {xx, yyy}}, '', color, 100, 100*gfx.opacity)
      end
    end
  end
  pgf.write_clipbox_end()
end

term.make_palette = function()
  -- continuous number of colours
  return 0
end

term.previous_palette = function()
  return 1
end

term.set_color = function(ctype, lt, value, opacity, r, g, b)
  gfx.check_in_path()
  -- FIXME gryscale on monochrome?? ... or use xcolor?

  if ctype == 'LT' then
    gfx.set_color('LT', {lt})
  elseif ctype == 'FRAC' then
    if gfx.opt.lines_colored then
      gfx.set_color('RGBA', {r, g , b, 1.0})
    else
      gfx.set_color('GRAY', {value})
    end
  elseif ctype == 'RGBA' then
    gfx.set_color('RGBA', {r, g , b, opacity})
  else
    gp.int_error(string.format("set color: unknown type (%s), lt (%i), value (%.3f)\n", ctype, lt, value))
  end
  
  return 1
end

-- Called when gnuplot is exited.
term.reset = function(p)
  gfx.check_in_path()
  gfx.check_variables()
  if gfx.opt.full_doc then
    pgf.write_doc_end()
  end
  return 1
end

--[[===============================================================================================

  command line code

]]--===============================================================================================

term_help = function(helptext)
  local w
  for w in string.gmatch(helptext, "([^\n]*)\n") do
    w = string.gsub(w, "\\", "\\\\")
    w = string.gsub(w, "\"", "\\\"")
    io.write('"'..w.."\",\n")
  end
--[[  
  local out = string.gsub(helptext, "\n", "\",\n\"")
  local out = string.gsub(helptext, "\n", "\",\n\"")
  io.write(out)]]
end

if arg then -- called from the command line!
  if #arg > 0 and arg[1] == 'style' then
    -- write style file
    pgf.create_style()
  elseif arg[1] == 'termhelp' then
    io.write([["2 lua tikz",
"?set terminal lua tikz",
"?set term lua tikz",
"?term lua tikz",
" The TikZ driver is one output mode of the generic Lua terminal.",
"",
" Syntax:",
"     set terminal lua tikz",
"",
]])
    pgf.print_help(term_help)
    io.write("\"\"\n")
  else
    io.write([[
 This script is intended to be called from GNUPLOT.

 For generating the associated TeX/LaTeX/ConTeXt style files
 just call this script with the additional option 'style':

   # lua gnuplot.lua style

 The TikZ driver provides the following additional terminal options:

]])
    pgf.print_help(io.write)
  end
end
