# pm3dCompress.awk - (c) Petr Mikulik; 1996/1997/1999/2002
#
# This awk script tries to compress a postscript file created by pm3d or
# gnuplot with pm3d splotting mode (limits: compresses only the first pm3d
# splot map there if in multiplot mode).
#
#
# Installation:
#	Copy this file into a directory listed in your AWKPATH 
# Running:
#	awk -f pm3dCompress.awk _original_.ps >reduced.ps
#
# Hint: make a script file/abbreviation/alias, e.g. under VMS
#	pm3dcompress:=="$disk:[dir]gawk.exe -f disk:[dir]pm3dCompress.awk"
#
# Note: use GNU awk whenever possible (HP awk does not have /dev/stderr, for
# instance).
#
#
# Strategy: this program browses the given .ps file and makes a list of all
# frequently used chains of postscript commands. If this list is not too large,
# then it replaces these chains by abbreviated commands.
#   This may reduce the size of the postscript file about 50 %, which is not
# so bad if you want to store these files for a later use (and with the same 
# functionality, of course). 
#
#
# (c) Petr Mikulik, mikulik@physics.muni.cz, http://www.sci.muni.cz/~mikulik/
#
# Distribution policy: this file must be distributed together with the 
# whole stuff of pm3d or gnuplot program.
#
# This is version: 2. 3. 2002
#     2. 3. 2002 - updated because of stroke in /g definition
#    15. 1. 1999 - new staff in pm3d and gnuplot/pm3d
#     9. 7. 1997 - update
#    16. 3. 1997 - first (?) version
#

BEGIN {
err = "/dev/stderr"
if (ARGC!=2) {
  print "pm3dCompress.awk --- (c) Petr Mikulik, Brno. Version 2. 3. 2002" >err
  print "Compression of pm3d .ps files. See the header of this file for more info." >err
  print "Usage:  awk -f pm3dCompress.awk inp_file.ps >out_file.ps" >err
  exit(1)
  }
getline
print "Please wait, it may take a while (input file "ARGV[1]" is read twice)." >err
while ($1!="%pm3d_map_begin") 
  if (getline<=0) { 
	print "No pm3d piece found in the input file, exiting." >err
	exit(7) }
PrintMapping=$0
nList=0

while ($1!="%pm3d_map_end") {
  if (getline<=0) {
	print "Corrupted pm3d block---end not found, exiting." >err
	exit(8) }
  i=index($0,"N");
  if (i>0) {
    S=substr($0,i,length($0))  # M including
    # is this string defined?
    for (i=0; i<nList && List[i]!=S; i++);
    if (i==nList) { # string is not in the list
      List[i]=S; nList++; 
      if (nList>300) {
	print "Defining more than 300 strings makes no sense, I think. Exiting, sorry." >err
	exit(2) }
      }
    } # for all NR
  }

print "List of frequent strings contains "nList" elements." >err

# now read the same file again the 3d part:
getline <ARGV[1]
while ($1!="%pm3d_map_begin") { 
  if ($1=="%%Creator:") 
    print $0", compressed by pm3dCompress.awk"
  else if ($1=="/pmdict") {
    $2+=nList; print }
  else print
  getline <ARGV[1]
  }

# now print the list as a new definition
print "\t\t\t\t\t%pm3dCompress definitions"
for (i=0; i<nList; i++) 
  print "/X"i" {"List[i]"} def"
for (i=0; i<nList; i++) {
  S=List[i]
  sub("N ","2 index g N ",S)
  print "/x"i" {"S"} def"
  }
print "\n"PrintMapping"\n"

# now substitute the new definitions: 
while ($1!="%pm3d_map_end") {
  getline <ARGV[1]; i=index($0,"N");
  if (i>0) {
    S=substr($0,i,length($0))  # M including
    # find the definition in the list
    for (m=0; m<nList && List[m]!=S; m++);
    S=substr($0,1,i-1)
    if (index(S," g ")) {
	gsub(" g "," ",S);
	S=S"x"m
	}
      else
	S=S"X"m
    print S
    }
    else print
  }
# read the rest of the file
flag = (getline <ARGV[1] ); 
while (flag>0) { print; flag = (getline <ARGV[1] ); }
close(ARGV[1])
}

# eof pm3dCompress.awk
