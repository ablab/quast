$! generates options file for vms link
$! p1 is filename and mode to open file (filename/write or filename/append)
$! p2 is comma-separated list of files
$
$ open file 'p1'
$ element=0
$loop:
$ x=f$element(element,",",'p2')
$ if x .eqs. "," then goto out
$ y=f$edit(x,"COLLAPSE")  ! lose spaces
$ if y .nes. "" then write file y
$ element=element+1
$ goto loop
$
$out:
$ close file
$ exit
