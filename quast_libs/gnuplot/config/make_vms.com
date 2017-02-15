$!
$! GNUPLOT make program for VMS, Vers. 1.2, 1996/07/03
$! (Rolf Niepraschk, niepraschk@ptb.de)
$!
$! This command procedure compiles and links GNUPLOT with MMS or MMK or
$! invokes the dcl procedure BUILDVMS.COM
$!
$! Usage: @MAKE_VMS P1 [P2]
$!    P1 = compiler ("DECC" or "VAXC" or "GNUC" or special make file if P2 = "")
$!         default is "DECC"
$!    P2 = special make file (eg. for testing), default is "DESCRIP.MMS"
$!
$ SAY = "WRITE SYS$OUTPUT"
$ AXP = 0
$ IF F$GETSYI("ARCH_TYPE") .NE. 1 THEN AXP = 1
$!
$ P1 = F$EDIT(P1,"UPCASE")
$ IF P2 .NES. ""
$ THEN D_FILE = P2
$ ELSE D_FILE = "MAKEFILE.VMS"
$ ENDIF
$!
$ COMPILER = ""
$ IF P1 .EQS. "DECC" .OR. P1 .EQS. ""
$ THEN
$   DECC = 1
$   COMPILER = "DECC"
$ ELSE
$   IF P1 .EQS. "VAXC"
$   THEN
$     VAXC = 1
$     COMPILER = "VAXC"
$   ELSE
$     IF P1 .EQS. "GNUC"
$     THEN
$       GNUC = 1
$       COMPILER = "GNUC"
$     ENDIF
$   ENDIF
$ ENDIF
$ IF COMPILER .EQS. ""
$ THEN
$   DECC = 1
$   COMPILER = "DECC"
$   D_FILE = P1
$ ENDIF
$!
$ DCL_PROC = "BUILDVMS.COM"
$ COMMAND = ""
$ IF F$TYPE(MMK) .NES. ""
$   THEN COMMAND = "MMK"
$ ELSE
$   IF F$SEARCH("SYS$SYSTEM:MMS.EXE") .NES. "" THEN COMMAND = "MMS"
$ ENDIF
$!
$ MACRO = ""
$ IF AXP THEN MACRO = "/MACRO=__ALPHA__=1"
$ IF COMMAND .NES. ""
$ THEN
$   SAY "Make Gnuplot with ''COMMAND' and ''COMPILER'."
$   SAY ""
$   'COMMAND' /DESCRIPTION='D_FILE' 'MACRO' /IGNORE=WARNING
$ ELSE
$   SAY "Make Gnuplot with DCL procedure ''DCL_PROC'."
$   SAY ""
$   @'DCL_PROC'
$ ENDIF
$ EXIT
