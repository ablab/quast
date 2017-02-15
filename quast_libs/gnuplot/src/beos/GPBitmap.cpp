
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GPBitmap.h"
#include "GPView.h"
#include "constants.h"
#include <Region.h>
#include <ScrollBar.h>

#define LineSolid 1


/*******************************************************************/
// GPBitmap
GPBitmap::GPBitmap(float width, float height)
{
	m_bitmap = new BBitmap(BRect(0,0,width-1,height-1),B_RGB_32_BIT,TRUE);
	BRect r(0,0,width-1,height-1);
	m_view = new BView(r,"BitmapDrawer",B_FOLLOW_ALL,0);
	m_bitmap->AddChild(m_view);
	max_commands	= 7;
	ncommands		= 0;
	m_needRedraw	= false;
	m_redrawing 	= false;
	commands		= (char **) malloc(max_commands * sizeof(char *));
	this->width		= nwidth = width;
	this->height	= nheight = height;

	for(int i=1 ; i<16 ; i++) {
		colors[i].red	= i*16;
		colors[i].green	= i*16;
		colors[i].blue	= i*16;
		colors[i].alpha	= 255;
	}
};

GPBitmap::GPBitmap(float width, float height, char **cmds)
{

}

GPBitmap::~GPBitmap()
{
	if (m_bitmap)
		delete m_bitmap;
	if(m_view)
		delete m_view;
	if(ncommands)
		clearCommands();
	if(commands) {
		free(commands);
		commands = NULL;
	}
}

void GPBitmap::SetDirty(BRegion *r)
{
	BMessage msg(bmsgBitmapDirty);
	m_needRedraw = true;
}

void GPBitmap::clearCommands()
{
	int32 ncmds = atomic_add(&ncommands,-ncommands);
	if(ncmds > 0) {
		for( --ncmds ; ncmds >= 0 ; ncmds--) {
			if(commands[ncmds])
				free(commands[ncmds]);
			commands[ncmds] = NULL;
		}
	}
}

#if 1
void GPBitmap::addCommands(BMessage *msg, int32 numCmds)
{
	char *p, **cmd = NULL;

	if (numCmds+ncommands >= max_commands) {
		max_commands = max_commands + numCmds + 2;
		commands = (commands)
			? (char **) realloc(commands, max_commands * sizeof(char *))
			: (char **) malloc(sizeof(char *));
	}
	if (!commands) {
		fputs("gnuplot: can't get memory. aborted.\n", stderr);
//		exit(1);
	} else {
		msg->FindPointer("cmds", (void **)&cmd);
//		printf("got : %X at %X\n",cmd[0], cmd);
		for(int i=0; i< numCmds; i++) {
//			cmd = msg->FindString("cmd", i);
//			p = (char *) malloc((unsigned) strlen(cmd[i]) + 1);
//			if (!p) {
//				fputs("gnuplot: can't get memory. aborted.\n", stderr);
//				exit(1);
//			} else
//				commands[ncommands++] = strcpy(p, cmd[i]);
			commands[ncommands++] = strdup(cmd[i]);
//			commands[ncommands++] = cmd[i];
			m_needRedraw = true;
		}
	}
}
#else
void GPBitmap::addCommands(BMessage *msg, int32 numCmds)
{
	char *cmd = NULL;

	if (numCmds+ncommands >= max_commands) {
		max_commands = max_commands + numCmds + 2;
		commands = (commands)
			? (char **) realloc(commands, max_commands * sizeof(char *))
			: (char **) malloc(sizeof(char *));
	}
	if (!commands) {
		fputs("gnuplot: can't get memory. aborted.\n", stderr);
//		exit(1);
	} else {
		for(int i=0; i< numCmds; i++) {
			cmd = msg->FindString("cmd", i);
			commands[ncommands++] = strdup(cmd);
			m_needRedraw = true;
		}
	}
}
#endif

void GPBitmap::addCommand(char *cmd)
{
//	printf("adding a cmd : %s\n",cmd);
	if(!cmd)
		return;
	if (ncommands >= max_commands) {
		max_commands = max_commands * 2 + 1;
		commands = (commands)
			? (char **) realloc(commands, max_commands * sizeof(char *))
			: (char **) malloc(sizeof(char *));
	}
	if (!commands) {
		fputs("gnuplot: can't get memory. X11 aborted.\n", stderr);
		return; // exit(1);
	} else {
		commands[ncommands++] = strdup(cmd);
		m_needRedraw = true;
	}
}

void GPBitmap::ResizeTo(float width, float height, uint32 btns)
{
	if(btns) {
		nwidth  = width;
		nheight = height;
//		m_needRedraw = true;
		return;
	}
	BRect r(0,0,width-1,height-1);
	if (m_bitmap)
		m_bitmap->Lock();
	if(m_view) {
		m_bitmap->RemoveChild(m_view);
		m_view->ResizeTo(nwidth, nheight);
	} else
		m_view = new BView(r,"BitmapDrawer",B_FOLLOW_ALL,0);

//    drawing_thread = spawn_thread(&drawing_loop, "gnuplot io_loop", B_LOW_PRIORITY, this); 
//    resume_thread(drawing_thread);
//	kill_thread(drawing_thread);

	if (m_bitmap) {
		m_bitmap->Unlock();
		delete m_bitmap;
	}

	m_bitmap = new BBitmap(r,B_RGB_32_BIT,TRUE);
	m_bitmap->Lock();
	m_bitmap->AddChild(m_view);
	this->width = nwidth = width;
	this->height = nheight = height;
	m_needRedraw = true;
}

int32 GPBitmap::drawing_loop(void* data)
{
	int32 res = 1;
	GPBitmap * bmp = (GPBitmap *)data;

	return res;
}

/*-----------------------------------------------------------------------------
 *   display - display a stored plot
 *---------------------------------------------------------------------------*/

void GPBitmap::display(float v_width, float v_height)
{
	int n, x, y, jmode, sl, lt = 0, type, point, px, py;
	int uwidth, user_width = 1;		/* as specified by plot...linewidth */
	char *buffer, *str;
	float sw, vchar;

	uint32 buttons;
	BPoint cursor;
	m_view->GetMouse(&cursor, &buttons);

	if(buttons == 0) {
		if ((width != v_width ) || (height != v_height))
			ResizeTo(v_width, v_height, 0);
	}

	if (ncommands == 0 || m_needRedraw == false)
		return;

	vchar = 11.0;
	m_view->SetFontSize(vchar);

	/* set scaling factor between internal driver & window geometry */
	xscale = width / 4096.0;
	yscale = height / 4096.0;

	/* initial point sizes, until overridden with P7xxxxyyyy */
	px = (int) (xscale * pointsize);
	py = (int) (yscale * pointsize);

	/* set pixmap background */
	m_view->SetHighColor(255,255,255);
	m_view->FillRect(BRect(0,0,width,height));
	m_view->SetViewColor(colors[2]);

	/* loop over accumulated commands from inboard driver */
	for (n = 0; n < ncommands; n++) {
		buffer = commands[n];

		/*   X11_vector(x,y) - draw vector  */
		if (*buffer == 'V') {
			sscanf(buffer, "V%4d%4d", &x, &y);
			m_view->StrokeLine( BPoint(X(cx), Y(cy)),BPoint(X(x), Y(y)));
			cx = x;
			cy = y;
		}
		/*   X11_move(x,y) - move  */
		else if (*buffer == 'M') {
			sscanf(buffer, "M%4d%4d", &cx, &cy);
			m_view->MovePenTo(BPoint(X(cx), Y(cy)));
		}
		/*   X11_put_text(x,y,str) - draw text   */
		else if (*buffer == 'T') {

			sscanf(buffer, "T%4d%4d", &x, &y);
			str = buffer + 9;
			sl = strlen(str) - 1;
			sw = m_view->StringWidth(str, sl);

			switch (jmode) {
				case CENTRE:				sw = -sw / 2;	break;
				case RIGHT:					sw = -sw;		break;
				case LEFT:					sw = 0;			break;
			}

			m_view->SetHighColor(colors[2]);
			m_view->DrawString(str, sl, BPoint(X(x) + sw, Y(y) + vchar / 3 ));
			m_view->SetHighColor(colors[lt + 3]);
		}
		else if (*buffer == 'F') {	/* fill box */
			int style, xtmp, ytmp, w, h;

			if (sscanf(buffer + 1, "%4d%4d%4d%4d%4d", &style, &xtmp, &ytmp, &w, &h) == 5) {
				/* gnuplot has origin at bottom left, but X uses top left
				 * There may be an off-by-one (or more) error here.
				 * style ignored here for the moment
				 */
                                /* ULIG: the style parameter is now used for the fillboxes style
                                 * (not implemented here), see the documentation */
				m_view->SetHighColor(colors[0]);
				m_view->FillRect(BRect(X(xtmp), Y(ytmp + h), w * xscale, h * yscale));
				m_view->SetHighColor(colors[lt + 3]);
			}
		}
		/*   X11_justify_text(mode) - set text justification mode  */
		else if (*buffer == 'J')
			sscanf(buffer, "J%4d", (int *) &jmode);

		/*  X11_linewidth(width) - set line width */
		else if (*buffer == 'W')
			sscanf(buffer + 1, "%4d", &user_width);

		/*   X11_linetype(type) - set line type  */
		else if (*buffer == 'L') {
			sscanf(buffer, "L%4d", &lt);
			lt = (lt % 8) + 2;
			/* default width is 0 {which X treats as 1} */
			uwidth = user_width; // widths[lt] ? user_width * widths[lt] : user_width;
//			if (dashes[lt][0]) {
//				type = LineOnOffDash;
//				XSetDashes(dpy, gc, 0, dashes[lt], strlen(dashes[lt]));
//			} else {
				type = LineSolid;
//			}
			m_view->SetHighColor(colors[lt + 3]);
			m_view->SetPenSize(uwidth);
			m_view->SetLineMode(B_ROUND_CAP,B_BEVEL_JOIN);
//			XSetLineAttributes(dpy, gc, width, type, CapButt, JoinBevel);
		}
		/*   X11_point(number) - draw a point */
		else if (*buffer == 'P') {
			/* linux sscanf does not like %1d%4d%4d" with Oxxxxyyyy */
			/* sscanf(buffer, "P%1d%4d%4d", &point, &x, &y); */
			point = buffer[1] - '0';
			sscanf(buffer + 2, "%4d%4d", &x, &y);
			if (point == 7) {
				/* set point size */
				px = (int) (x * xscale * pointsize);
				py = (int) (y * yscale * pointsize);
			} else {
				if (type != LineSolid || uwidth != 0) {	/* select solid line */
					m_view->SetPenSize(1.0);
					m_view->SetLineMode(B_ROUND_CAP,B_BEVEL_JOIN);
//					XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinBevel);
				}
				switch (point) {
					case 0:	/* dot */			doDot(x,y);				break;
					case 1:	/* do diamond */	doDiamond(x,y,px,py);	break;
					case 2:	/* do plus */		doPlus(x,y,px,py);		break;
					case 3:	/* do box */		doBox(x,y,px,py);		break;
					case 4:	/* do X */			doCross(x,y,px,py);		break;
					case 5:	/* do triangle */	doTriangle(x,y,px,py);	break;
					case 6:	/* do star */		doStar(x,y,px,py);		break;
				}
				if (type != LineSolid || uwidth != 0) {	/* select solid line */
					m_view->SetPenSize(uwidth);
//					canview->SetLineMode(B_ROUND_CAP,B_ROUND_JOIN);
//				    XSetLineAttributes(dpy, gc, width, type, CapButt, JoinBevel);
				}
			}
		} /* end  X11_point(number) - draw a point */
	} /* end loop over accumulated commands from inboard driver */ 
	m_view->Sync();
	m_needRedraw = false;
}

void GPBitmap::doDiamond(int x, int y, int px, int py) {
	m_view->StrokeLine(BPoint(X(x) - px, Y(y)),BPoint(X(x), Y(y) - py));
	m_view->StrokeLine(BPoint(X(x) + px, Y(y)));
	m_view->StrokeLine(BPoint(X(x), Y(y) + py));
	m_view->StrokeLine(BPoint(X(x) - px, Y(y)));
	m_view->StrokeLine(BPoint(X(x), Y(y)),BPoint(X(x), Y(y)));
}

void GPBitmap::doPlus(int x, int y, int px, int py) {
	m_view->StrokeLine(BPoint(X(x) - px, Y(y)),BPoint(X(x) + px, Y(y)));
	m_view->StrokeLine(BPoint(X(x), Y(y) - py),BPoint(X(x), Y(y) + py));
}

void GPBitmap::doBox(int x, int y, int px, int py) {
	m_view->StrokeRect(BRect(X(x) - px, Y(y) - py, (px + px), (py + py)));
	m_view->StrokeLine(BPoint(X(x), Y(y)),BPoint(X(x), Y(y)));
}

void GPBitmap::doCross(int x, int y, int px, int py) {
	m_view->StrokeLine(BPoint(X(x) - px, Y(y) - py),BPoint(X(x) + px, Y(y) + py));
	m_view->StrokeLine(BPoint(X(x) - px, Y(y) + py),BPoint(X(x) + px, Y(y) - py));
}

void GPBitmap::doTriangle(int x, int y, int px, int py) {
	short temp_x, temp_y;

	temp_x = (short) (1.33 * (double) px + 0.5);
	temp_y = (short) (1.33 * (double) py + 0.5);

	m_view->StrokeLine(BPoint(X(x), Y(y) - temp_y), BPoint(X(x) + temp_x, Y(y) - temp_y + 2 * py));
	m_view->StrokeLine(BPoint(X(x) - temp_x, Y(y) - temp_y + 2 * py));
	m_view->StrokeLine(BPoint(X(x), Y(y) - temp_y));
	m_view->StrokeLine(BPoint(X(x), Y(y)),BPoint(X(x), Y(y)));
}

void GPBitmap::doStar(int x, int y, int px, int py) {
	m_view->StrokeLine(BPoint(X(x)-px, Y(y)), BPoint(X(x) + px, Y(y)));
	m_view->StrokeLine(BPoint(X(x), Y(y)-py), BPoint(X(x), Y(y)+py));
	m_view->StrokeLine(BPoint(X(x)-px, Y(y)-py), BPoint(X(x)+px, Y(y)+py));
	m_view->StrokeLine(BPoint(X(x)-px, Y(y)+py), BPoint(X(x)+px, Y(y)-py));
}

void GPBitmap::doDot(int x, int y) {
	m_view->StrokeLine(BPoint(X(x), Y(y)),BPoint(X(x), Y(y)));
}

