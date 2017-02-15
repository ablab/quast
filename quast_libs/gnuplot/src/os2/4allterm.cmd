/*  4allterm.cmd
	This is an replacement for the unix shell script that makefiles normally
	use to extract and sort terminal help from .trm files
	It does only use cmd.exe as external tool and is much faster than
	the previous approach.
*/

allterm = "..\docs\allterm.h"
term_prefix = "..\term\"

parse arg args
sort_list = 1
if args = "nosort" then do
	sort_list = 0
end
else if args \= "sort" then do
	say "4allterm creates ..\term\allterm.h"
	say "Please specify `sort` or `nosort` options!"
	return 0
end


/* extract all terminal files from makefile.all,
   extract terminal names from terminal files, store result in terminal.
   extract 
*/
m = "makefile.all"
i = 0
lf = "0A"x
start_help = lf || "START_HELP("
end_help   = lf || "END_HELP("
call stream m, "c", "open read"
do while lines(m) > 0
	l = linein(m)
	do while l \= '' 
		parse var l "$(T)" term l
		if term \= '' then do

			/* read complete .trm file */
			term  = term_prefix || term
			size = stream(term, "c", "query size")
			call stream term, "c", "open read"
			data = charin(term, 1, size)
			call stream term, "c", "close"

			/* find help section */
			p = pos( start_help, data )
			do while p > 0
				/* get terminal name, store it in terminal.i */
				s = p + length(start_help)
				q = pos(")", data, s)
				i = i + 1
				term_name = substr(data, s, q-p)
				terminal.i = term_name

				/* find end of help text */
				q = pos(end_help, data, q+1)
				q = pos(")", data, q)

				/* save help text in help., use stem help. as hash table */
				help.term_name = substr(data, p+1, q-p)

				/* find next help section */
				p = pos(start_help, data, q)
			end
		end
	end
end
call stream m, "c", "close"
terminal.0 = i
drop q s p m l term_name term lf start_help end_help size data


/* sort list of terminals 
*/
/* simple bubble sort copied from 
   Bernd Schemmer's "Rexx Tips'N Tricks" v3.5 */
if sort_list then do
	do i = terminal.0 to 1 by -1 until flip_flop = 1
		flip_flop = 1
		do j = 2 to i
			m = j - 1
			if translate(terminal.m) >> translate(terminal.j) then
			do
				xchg       = terminal.m
				terminal.m = terminal.j
				terminal.j = xchg
				flip_flop  = 0
			end
		end
	end
end


/*  create allterm.h 
*/
address "cmd" "@del" allterm "2>out" /* remove old file */
do i = 1 to terminal.0
	term = terminal.i
	call lineout allterm, help.term
end

return 0

