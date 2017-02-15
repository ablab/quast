This is gnuplot version 5.0 -- binary distribution for Windows
===========================================================================

gnuplot is a command-line driven interactive function plotting utility
for linux, OSX, Windows, VMS, and many other platforms.  The software is
copyrighted but freely distributed (i.e., you don't have to pay for it).
It was originally intended as graphical program to allow scientists
and students to visualize mathematical functions and data.

gnuplot handles both curves (2 dimensions) and surfaces (3 dimensions).
Surfaces can be plotted as a mesh fitting the specified function, floating
in the 3-d coordinate space, or as a contour plot on the x-y plane.
For 2-d plots, there are also many plot styles including lines, points,
boxes, heat maps, stacked histograms, and contoured projections of 3D data.
Graphs may be labeled with arbitrary labels and arrows, axis labels,
a title, date and time, and a key.


Getting started
---------------

The new gnuplot user should begin by reading the general information
available by typing `help` after running gnuplot. Then read about the
`plot` command (type `help plot`).  The manual for gnuplot (which is a
nicely formatted version of the on-line help information) is available
as a PDF document.

You can find loads of test and sample scripts in the 'demo' directory.
Try executing `test` and `load "all.dem"` or have a look at the online
version of the demos at
  http://www.gnuplot.info/screenshots/index.html#demos


License
-------

See the Copyright file for copyright conditions.

The "GNU" in gnuplot is NOT related to the Free Software Foundation,
the naming is just a coincidence (and a long story; see the gnuplot FAQ
for details). Thus gnuplot is not covered by the GPL (GNU Public License)
copyleft, but rather by its own copyright statement, included in all source
code files. However, some of the associated drivers and support utilities
are dual-licensed.


gnuplot binaries
----------------

* wgnuplot.exe:  GUI version and the default gnuplot executable. As of version 5 
  it emulates pipe functionality.
  
* wgnuplot_pipes.exe:  Variant with full pipe functionality at the expense of
  an additional console window.

* gnuplot.exe:  Text (console) mode version of the gnuplot executable with full
  pipe functionality as it is common on other platforms. In contrast to
  wgnuplot.exe, this program can also accept commands on stdin (standard input)
  and print messages on stdout (standard output). It replaces pgnuplot.exe and
  is recommended to be used with 3rd party applications using gnuplot as graph
  engine, like e.g. Octave (www.octave.org).

* pgnuplot.exe:  This helper program is no longer shipped with binary package.
  Use gnuplot.exe instead.

* runtime library files
  Runtime library files (e.g. freetype6.dll) that are required for gnuplot
  are included in the package.  Licenses of these runtime libraries can be
  found in the 'license' directory.


Installation
------------

gnuplot comes with its own installer, which will basically do the following,
provided you check the corresponding options:

* Extract this package (or parts thereof) in a directory of your coice, e.g.
  C:\Program Files\gnuplot etc.

* Create shortcut icons to wgnuplot on your dektop and (on Windows XP and
  Vista) within the Quick-Lauch area. Additionally, a menu is added to the
  startup menu with links to the programs, help and documentation,
  gnuplot's internet site and the demo scripts.

* The extensions *.gp, *.gpl, *.plt will be associated to be opened with
  wgnuplot. To change file associations in Windows 7 or Vista, go to
  "Control Panel", "Control Panel Home", "Default Programs",
  "Set Associations". Select a file type in the list and click
  "Change Program".

* The path to the gnuplot binaries is added to the PATH environment variable.
  That way you can start gnuplot by typing `gnuplot' or `wgnuplot' on a command
  line.

* gnuplot is added to the shortcuts of the Windows explorer "Run" Dialog.
  To start wgnuplot simply press Windows-R and execute `wgnuplot'.

* You may select your default terminal of preference (wxt/windows) and the
  installer will update the GNUTERM environment variable accordingly. See below
  on how to change environment variables. Alternatively, you can later add
    set term windows
  or
    set term wxt
  to your gnuplot.ini, see `help startup`.

* If you install the demo scripts, the directory containing the demos is
  included in the GNUPLOT_LIB search path, see below.

Customisation:
On startup, gnuplot executes the gnuplot.ini script from the user's
application data directory %APPDATA% if found, see `help startup`. wgnuplot
and the windows terminal load and save settings from/to wgnuplot.ini located
in the appdata directory, see `help wgnuplot.ini`.


Fonts
-----

graphical text window (wgnuplot.exe):
  You can change the font of the terminal window by selecting "Options..." -
  "Choose Font..." via the toolbar or the context (right-click) menu. We
  strongly encourage you to use a modern Truetype font like e.g. "Consolas"
  instead of the old "Terminal" font, which was the default until gnuplot
  version 4.4. Make sure to "Update wgnuplot.ini" to make this change
  permanent.

console window (gnuplot.exe):
  If extended characters do not display correctly you might have to change
  the console font to a non-raster type like e.g. "Consolas" or "Lucida
  Console". You can do this using the "Properties" dialog of the console
  window.


Localisation
------------

As of version 4.6 gnuplot supports localised versions of the menu and help
files. By default, gnuplot tries to load wgnuplot-XX.mnu and wgnuplot-XX.chm,
where XX is a two character language code. Currently, only English (default)
and Japanese (ja) are supported, but you are invited to contribute.

You can enforce a certain language by adding
  Language=XX
to your wgnuplot.ini. This file is located in your %APPDATA% directory. If you
would like to have mixed settings, e.g, English menus but Japanese help texts,
you could add the following statements to your wgnuplot.ini:
  HelpFile=wgnuplot-ja.chm
  MenuFile=wgnuplot.mnu

Please note that currently there's no way to change the language setting from
within gnuplot.


Environmental variables
-----------------------

For a complete list of environment variables supported, type `help environment`
in gnuplot.

To set/change environment variables go to "Control panel", "System",
("Advanced"), "Environmental variables" on Windows NT/2000/XP/Vista, or Right
click on the Computer icon on your Desktop and choose Properties option,
"System", "Advanced system settings", "Advanced", "Environmental variables"
on Windows 7.

* If GNUTERM is defined, it is used as the name of the terminal type to be
  used. This overrides any terminal type sensed by gnuplot on start-up, but is
  itself overridden by the gnuplot.ini start-up file (see `help startup`) and,
  of course, by later explicit changes.

* Variable GNUPLOT_LIB may be used to define additional search directories for
  data and command files. The variable may contain a single directory name, or
  a list of directories separated by a path separator ';'. The contents of
  GNUPLOT_LIB are appended to the `loadpath` variable, but not saved with the
  `save` and `save set` commands. See 'help loadpath' for more details.

* Variable GNUFITLOG holds the name of a file that saves fit results. The
  default it is fit.log. If the name ends with a "/" or "\", it is treated as a
  directory name, and "fit.log" will be created as a file in that directory.


Known bugs
----------

Please see and use

    http://sourceforge.net/p/gnuplot/bugs/

for an up-to-date bug tracking system.

--------------------------------------------------------------------------------

The gnuplot team, February 2015
