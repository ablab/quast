#define DIALOGS_H

#define IDM_HELPINDEX         10004
#define IDM_KEYSHELP          10003
#define IDM_EXTENDEDHELP      10002
#define IDM_HELPFORHELP       10001
#define IDM_GPLOTINF          10005

#define IDM_FILE              100
#define IDM_PRINTSETUP        115
#define IDM_PRINTPIC          111
#define IDM_PRINT             101
#define IDM_EXIT              102
#define IDM_ABOUT             103
#define IDM_FONTS             104
#define IDM_SAVE              105

#define IDM_COMMAND           300

#define IDM_CONTINUE          400

#define IDM_PAUSEOPT          120
#define IDM_PAUSEDLG          121
#define IDM_PAUSEBTN          122
#define IDM_PAUSEGNU          123

#define IDM_OPTIONS           200
#define IDM_OPTIONMAIN        201
#define IDM_COLOURS           206
#define IDM_LINES             207
#define IDM_LINES_THICK       208
#define IDM_LINES_SOLID       209
#define IDM_FRONT             210
#define IDM_KEEPRATIO         211

#define IDM_EDIT              500
#define IDM_COPY              501
#define IDM_CUT               502
#define IDM_PASTE             503
#define IDM_CLEARCLIP         504

#define IDM_MOUSE             600
#define IDM_USEMOUSE          601
/*
#define IDM_MOUSE_COORDINATES 602
#define IDM_MOUSE_COORDINATES_REAL    603
#define IDM_MOUSE_COORDINATES_PIXELS  604
#define IDM_MOUSE_COORDINATES_SCREEN  605
#define IDM_MOUSE_COORDINATES_XDATE   606
#define IDM_MOUSE_COORDINATES_XTIME   607
#define IDM_MOUSE_COORDINATES_XDATETIME       608
*/
#define IDM_MOUSE_UNZOOM              610
#define IDM_MOUSE_UNZOOMALL           611
#define IDM_MOUSE_ZOOMNEXT            612
#define IDM_MOUSE_RULER                       613
/*#define IDM_MOUSE_RULERWINDOW		614 */
#define IDM_MOUSE_POLAR_DISTANCE      615
#define IDM_MOUSE_CMDS2CLIP           616
#define IDM_MOUSE_FORMAT		620 /* keep this order of *_FORMAT_* constants! */
#define IDM_MOUSE_FORMAT_X_Y          621
#define IDM_MOUSE_FORMAT_XcY          622
#define IDM_MOUSE_FORMAT_XsY          623
#define IDM_MOUSE_FORMAT_XcYc         624
#define IDM_MOUSE_FORMAT_XcYs         625
#define IDM_MOUSE_FORMAT_pXdYp                626
#define IDM_MOUSE_FORMAT_pXcYp                627
#define IDM_MOUSE_FORMAT_pXsYp                628
#define IDM_MOUSE_FORMAT_LABEL                629
#define IDM_MOUSE_FORMAT_TIMEFMT	630
#define IDM_MOUSE_FORMAT_DATE		631
#define IDM_MOUSE_FORMAT_TIME		632
#define IDM_MOUSE_FORMAT_DATETIME	633
#define IDM_MOUSE_HELP			640

#define IDM_UTILS             650
#define IDM_BREAK_DRAWING     651
#define IDM_SET_GRID          652
#define IDM_SET_LINLOGY               653
#define IDM_SET_AUTOSCALE     655
#define IDM_DO_REPLOT         656
#define IDM_DO_RELOAD         657
#define IDM_DO_SENDCOMMAND    658

#define IDM_SET                       700
#define IDM_SET_D_S		710 /* 'set data style' options */
#define IDM_SET_D_S_BOXES	711 /* must be the same order as SetDataStyles[] */
#define IDM_SET_D_S_DOTS      712
#define IDM_SET_D_S_FSTEPS    713
#define IDM_SET_D_S_HISTEPS   714
#define IDM_SET_D_S_IMPULSES  715
#define IDM_SET_D_S_LINES     716
#define IDM_SET_D_S_LINESPOINTS       717
#define IDM_SET_D_S_POINTS    718
#define IDM_SET_D_S_STEPS     719
#define IDM_SET_F_S		730 /* 'set function style' options */
#define IDM_SET_F_S_BOXES	731 /* must be the same order as SetDataStyles[] */
#define IDM_SET_F_S_DOTS      732
#define IDM_SET_F_S_FSTEPS    733
#define IDM_SET_F_S_HISTEPS   734
#define IDM_SET_F_S_IMPULSES  735
#define IDM_SET_F_S_LINES     736
#define IDM_SET_F_S_LINESPOINTS       737
#define IDM_SET_F_S_POINTS    738
#define IDM_SET_F_S_STEPS     739

#define ID_ABOUT              10
#define IDD_COLOURS           20

#define IDD_PRINTQNAME        5106
#define IDD_PRINTSETUP        5105
#define IDD_PRINTPIC          5101
#define ID_PRINT              5100
#define ID_QPRINT             5000
#define ID_QPRINTERS          5200
#define ID_PRINTSTOP          5300
#define IDD_QPRSLIST          5201
#define IDD_QPRSSEL           5202
#define IDD_QPRNAME           5009
#define IDD_QPRFRAME          5008
#define IDD_QPRTRACK          5007
#define IDD_QPRSETPR          5006
#define IDD_QPRXSIZE          5001
#define IDD_QPRYSIZE          5002
#define IDD_QPRXFRAC          5003
#define IDD_QPRYFRAC          5004
#define IDD_QPRBOX            5005

#define IDD_FONTS             6000

#define IDD_PAUSEBOX          3000
#define IDD_PAUSETEXT         3001

#define IDD_QUERYPRINT              2000
#define IDD_QPTEXT                  2001
#define IDD_QPLISTBOX               2002
#define IDD_QPJOBPROP               2005


/*
#define DID_HELP     10
#define DID_CANCEL   2
#define DID_OK       1
*/
#define IDH_EXTENDED          905
#define IDH_INDEX             904
#define IDH_KEYS              903
#define IDH_FOREXTENDED       902
#define IDH_FORHELP           901

#define IDD_PRINTNAME               5010

/* Cursors */
#define IDP_CROSSHAIR  651
#define IDP_SCALING    652
#define IDP_ROTATING   653
