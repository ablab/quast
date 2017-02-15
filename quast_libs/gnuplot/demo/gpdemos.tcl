#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

# gpdemos.tcl --
#
# Demostration of gnuplot demos under Tcl/Tk.  Adapted from orignal
# demo by D.M.Burns and image demo from Tcl/Tk.  To run under unix:
# 
#    unix> wish gpdemos.tcl
#
# or if gpdemos.tcl is changed to executable:
#
#    unix> gpdemos.tcl
#
# Starting directory for demos is taken from environmental variable
# GNUPLOT_LIB.
#
#  9 Sep 2004 - Original demo, Donald M. Burns
# 12 Sep 2004 - Enhanced to search directory for *.dem files.

# TAILOR THIS PATH IF NECESSARY
set gnuplot    gnuplot

if {[info exists env(GNUPLOT_LIB)]} {
        set gp_library $env(GNUPLOT_LIB)
} else {
        # the environment var isn't set, default to current directory
        set gp_library .
}

set demopath $gp_library

eval destroy [winfo child .]

wm protocol . WM_DELETE_WINDOW safe_exit

# Open up a pipe to gnuplot
set gpfd [open "|$gnuplot" w+]
fconfigure $gpfd -buffering none -blocking 0

# A procedure to give commands to gnuplot
proc gnuplot {a} {
    global gpfd

    fileevent $gpfd writable [puts $gpfd $a]
}

# A procedure to clean-up on exit - this is broken for windows!
proc safe_exit {} {
    global gpfd

    set pids [pid $gpfd]
    foreach pid $pids {
	exec kill $pid
    }
    close $gpfd
    exit
}

# loadDir --
# This procedure reloads the directory listbox from the contents 
# of the directory named in the demo's entry.
proc loadDir {} {
    global dirName

    .sel.f.list delete 0 end
    foreach i [lsort [glob -nocomplain [file join $dirName *.dem]]] {
	.sel.f.list insert end [file tail $i]
    }
    foreach i [lsort [glob -nocomplain [file join $dirName *.gp]]] {
	.sel.f.list insert end [file tail $i]
    }
}

# loadImage --
# Given the name of the toplevel window of the demo and the mouse
# position, extracts the directory entry under the mouse and loads
# that file into a photo image for display.
#
# Arguments:
# x, y-			Mouse position within the listbox.
proc loadDemo {x y} {
    global dirName
    global gpfd

    # Send an ctrl-C to gnuplot in case a different demo is running.
    exec kill -INT [pid $gpfd]

    set file [file join $dirName [.sel.f.list get @$x,$y]]
    puts stderr "Loading demo $file"

    # Some demos may have set terminal and failed to set it back.
    gnuplot "\nset term x11; reset; load \"$file\""
}

set font {Helvetica 11}

wm title . "Tcl/Tk Gnuplot Demonstration"
wm iconname . "Tcl/Tk GP"

frame .sel
frame .sep -relief ridge -bd 1 -width 2
frame .plt
pack .sel -side left -fill y -padx 10 -pady 10 -expand no
pack .sep -side left -fill y -expand no
pack .plt -side right -fill both -expand yes -padx 10 -pady 10

label .sel.msg -font $font -wraplength 150 -justify left -text "This is an example of using gnuplot to draw into an X window opened by an external application. It allows you to run the demo listed demo scripts individually."
pack .sel.msg -side top

## Create selection side of window
frame .sel.buttons
pack .sel.buttons -side bottom -fill both -expand no
button .sel.buttons.quit -text Quit -command "safe_exit"
pack .sel.buttons.quit -side left -expand 1 -fill both
#
label .sel.dirLabel -text "Directory:"
set dirName [file join $gp_library]
entry .sel.dirName -textvariable dirName
bind .sel.dirName <Return> "loadDir"
frame .sel.spacer1 -height 3m
label .sel.fileLabel -text "File:"
frame .sel.f
frame .sel.spacer2 -height 2m
pack .sel.dirLabel -side top -anchor w
pack .sel.dirName -side top -fill x
pack .sel.spacer1 .sel.fileLabel -side top -anchor w
pack .sel.f -fill both -expand 1
pack .sel.spacer2 -side top
#
listbox .sel.f.list -yscrollcommand ".sel.f.scroll set"
scrollbar .sel.f.scroll -command ".sel.f.list yview"
pack .sel.f.list -side left -fill both -expand 1
pack .sel.f.scroll -side right -fill y -expand 0
bind .sel.f.list <Double-1> "loadDemo %x %y"

## Run load directory routine to fill list box
loadDir

## Create the plot side of window
frame .plt.g -bg "" -width 640 -height 450
frame .plt.spacer2 -height 2m
frame .plt.buttons
button .plt.buttons.bnext -text Next -command {gnuplot ""}
button .plt.buttons.bstop -text Reset -command {exec kill -INT [pid $gpfd]; gnuplot "\nreset; clear"}
#
pack .plt.buttons.bnext .plt.buttons.bstop -side left -expand 1
pack .plt.buttons -side bottom -fill both -expand no
pack .plt.spacer2 -side bottom
pack .plt.g -side top -expand true -fill both

# Tell gnuplot to use our frame .plt.g for display
gnuplot "set term x11 window '[winfo id .plt.g]'"
gnuplot "clear"
