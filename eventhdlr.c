
#include <stdio.h>
/*
 * stdlib.h was hpux-only; without it bsearch() is implicitly "int"
 * and its returned pointer gets truncated to 32 bits on 64-bit hosts.
 */
#include <stdlib.h>
#include <string.h>	/* strcasecmp */
#include <strings.h>

#include <X11/Intrinsic.h>	
#include <X11/StringDefs.h>
#include <X11/Xatom.h>	/* XA_PRIMARY, XA_STRING, XA_ATOM */


#include <X11/Xaw/Label.h>	
#include <X11/Xaw/Cardinals.h>	
#include <X11/Xaw/Box.h>	


#include "x3270.h"
#include "ColorScreen.h"	/* XawColScrXyToCell */
#include "3270_enc.h"
#include "global.h"
#include "ibm3270.h"
#include "fs3270.h"
#include "keyfunctions.h"
#include "dbug.h"

extern unsigned char atoe[];
int xcurs,ycurs;   
static struct key {
  char *name;
  int (*func)();
} keys[] = {
    { "backspace", fs_backspace },
    { "backtab", fs_backtab },
    { "clear", fs_clear },
    { "cursor_alt", fs_cursor_alt },
    { "cursor_blink", fs_cursor_blink },
    { "delete", fs_delete },
    { "down", fs_down },
    { "dup", fs_dup },
    { "enter", fs_enter },
    { "eraseeof", fs_eraseeof },
    { "eraseinput", fs_eraseinput },
    { "fieldmark", fs_fieldmark },
    { "home", fs_home },
    { "insertmode", fs_insertmode },
    { "left", fs_left },
    { "mono_case", fs_mono_case },
    { "pa1", fs_pa1 },
    { "pa2", fs_pa2 },
    { "pa3", fs_pa3 },
    { "penselect", fs_penselect },
    { "pf1", fs_pf1 },
    { "pf10", fs_pf10 },
    { "pf11", fs_pf11 },
    { "pf12", fs_pf12 },
    { "pf13", fs_pf13 },
    { "pf14", fs_pf14 },
    { "pf15", fs_pf15 },
    { "pf16", fs_pf16 },
    { "pf17", fs_pf17 },
    { "pf18", fs_pf18 },
    { "pf19", fs_pf19 },
    { "pf2", fs_pf2 },
    { "pf20", fs_pf20 },
    { "pf21", fs_pf21 },
    { "pf22", fs_pf22 },
    { "pf23", fs_pf23 },
    { "pf24", fs_pf24 },
    { "pf3", fs_pf3 },
    { "pf4", fs_pf4 },
    { "pf5", fs_pf5 },
    { "pf6", fs_pf6 },
    { "pf7", fs_pf7 },
    { "pf8", fs_pf8 },
    { "pf9", fs_pf9 },
    { "reset", fs_reset },
    { "return", fs_return },
    { "right", fs_right },
    { "sysreq", fs_sysreq },
    { "tab", fs_tab },
    { "up", fs_up },
    
  };

extern int strcasecmp();
static 
  keycomp(k1,k2)
struct key *k1,*k2;
{
  return strcasecmp(k1->name,k2->name);
}

int 
(*str_to_func(s))()
     char *s;
{
  struct key *ak,search_key;
  
  search_key.name = s;
  ak = (struct key *) 
    bsearch(&search_key,keys,sizeof(keys)/sizeof(keys[0]),
	    sizeof(keys[0]), keycomp);
  if (ak == NULL) 
    return NULL;
  else
    return ak->func;
}

#ifdef TEST
main(argc,argv)
char **argv;
{
	fprintf(stderr,"aid of %s is %d\n", argv[1],str_to_aid(argv[1]));
}
#endif





void 
HandleKeyPressed( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  KeyPressed \n"));
}

void 
HandleStringEvent( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  char ch;

  DBUG_PRINT("events",("Event StringEvent \n"));
  ch=*params[0];
  if (ch == NULL) ch = 0x22;
  fs_character(ch);
}

void 
HandleEnterWindow( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event EnterWindow \n"));
  status_disp(10, CG_BLANK);
  status_disp(11, CG_BLANK);
}

void 
HandleLeaveWindow( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event LeaveWindow \n"));
  status_disp(10, CG_CLOCKLEFT);
  status_disp(11, CG_CLOCKRIGHT);

}

void 
HandleFocusChange( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event FocusChange \n"));
}

void 
HandleKeymapChange( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event Keymap ChChange \n"));
}

/*
 * ---- Copy and paste through X selections ----
 *
 * Mouse:  dragging with button 1 selects a rectangle of screen cells
 *         and offers its text as PRIMARY; a click without dragging just
 *         moves the cursor.  Button 2 pastes.
 * Keys:   copy-selection() offers the selected text as CLIPBOARD (bound
 *         to Cmd-C and Ctrl+Shift+C); insert-selection(SEL, ...) pastes
 *         the first named selection that has text (Cmd-V, Ctrl+Shift+V,
 *         Shift+Insert, button 2).
 *
 * Text is kept as Latin-1, the character set of the screen font and of
 * the code page tables (asc_ebc.c), and is offered and requested as
 * UTF8_STRING or STRING.  The screen side (highlight, reading cells,
 * typing pasted text) is in fs3270.c.
 */

#define SEL_TEXT_MAX	16384	/* > 43 rows x (132 columns + newline) */

static char	*primary_text = NULL;	/* text we offer as PRIMARY */
static char	*clipboard_text = NULL;	/* text we offer as CLIPBOARD */
static int	selecting = FALSE;	/* button 1 went down on the screen */
static int	owning = FALSE;		/* inside our own XtOwnSelection */

static Atom atom_named (w, name)
     Widget	w;
     char	*name;
{
  return XInternAtom(XtDisplay(w), name, False);
}

/* the screen cell under the pointer of a button or motion event */
static void event_cell (w, event, r, c)
     Widget	w;
     XEvent	*event;
     int	*r, *c;
{
  int x, y;

  if (event->type == MotionNotify) {
    x = event->xmotion.x;
    y = event->xmotion.y;
  } else {
    x = event->xbutton.x;
    y = event->xbutton.y;
  }
  XawColScrXyToCell(w, x, y, r, c);
}

/* Latin-1 -> newly XtMalloc'ed UTF-8; *outlen gets its length */
static char *latin1_to_utf8 (s, outlen)
     unsigned char	*s;
     unsigned long	*outlen;
{
  char *out = XtMalloc(strlen((char *) s) * 2 + 1);
  unsigned long n = 0;

  for (; *s; s++) {
    if (*s < 0x80)
      out[n++] = *s;
    else {				/* 0x80..0xff: two bytes */
      out[n++] = 0xc0 | (*s >> 6);
      out[n++] = 0x80 | (*s & 0x3f);
    }
  }
  out[n] = '\0';
  *outlen = n;
  return out;
}

/*
 * UTF-8 -> Latin-1, in a buffer of at least len bytes.  Characters
 * outside Latin-1 (and broken sequences) become '?'.  Returns the length.
 */
static int utf8_to_latin1 (in, len, out)
     unsigned char	*in;
     int		len;
     unsigned char	*out;
{
  int i = 0, n = 0, extra;
  unsigned long cp;

  while (i < len) {
    unsigned char b = in[i];

    if (b < 0x80) {			/* plain ASCII */
      out[n++] = b;
      i++;
      continue;
    }
    /* lead byte gives the number of continuation bytes */
    if ((b & 0xe0) == 0xc0)
      extra = 1, cp = b & 0x1f;
    else if ((b & 0xf0) == 0xe0)
      extra = 2, cp = b & 0x0f;
    else if ((b & 0xf8) == 0xf0)
      extra = 3, cp = b & 0x07;
    else {				/* stray continuation byte */
      out[n++] = '?';
      i++;
      continue;
    }
    if (i + extra >= len) {		/* continuation bytes missing */
      out[n++] = '?';
      break;
    }
    for (i++; extra > 0; extra--, i++)
      cp = (cp << 6) | (in[i] & 0x3f);
    out[n++] = cp <= 0xff ? cp : '?';
  }
  return n;
}

/* Xt convert procedure: hand our PRIMARY or CLIPBOARD text to a client */
static Boolean convert_selection (w, selection, target, type, value,
				  length, format)
     Widget		w;
     Atom		*selection, *target, *type;
     XtPointer		*value;
     unsigned long	*length;
     int		*format;
{
  char *text = *selection == XA_PRIMARY ? primary_text : clipboard_text;
  Atom utf8 = atom_named(w, "UTF8_STRING");
  Atom targets = atom_named(w, "TARGETS");
  Atom textatom = atom_named(w, "TEXT");

  if (text == NULL)
    return False;
  if (*target == targets) {		/* what we can convert to */
    Atom *t = (Atom *) XtMalloc(4 * sizeof (Atom));

    t[0] = targets;
    t[1] = utf8;
    t[2] = XA_STRING;
    t[3] = textatom;
    *type = XA_ATOM;
    *value = (XtPointer) t;
    *length = 4;
    *format = 32;
    return True;
  }
  if (*target == utf8) {
    *value = (XtPointer) latin1_to_utf8((unsigned char *) text, length);
    *type = utf8;
    *format = 8;
    return True;
  }
  if (*target == XA_STRING || *target == textatom) {
    *length = strlen(text);
    *value = (XtPointer) XtNewString(text);
    *type = XA_STRING;
    *format = 8;
    return True;
  }
  return False;
}

/* another client took a selection from us: drop the highlight for
 * PRIMARY (like xterm); a lost CLIPBOARD needs nothing */
static void lose_selection (w, selection)
     Widget	w;
     Atom	*selection;
{
  if (*selection == XA_PRIMARY && !owning)
    fs_select_clear();
}

/* offer the selected screen text as selection sel; FALSE if none */
static int own_selection (w, sel, store)
     Widget	w;
     Atom	sel;
     char	**store;
{
  static char buf[SEL_TEXT_MAX];

  if (!fs_select_active())
    return FALSE;
  fs_select_text(buf, sizeof buf);
  XtFree(*store);
  *store = XtNewString(buf);
  owning = TRUE;
  XtOwnSelection(w, sel, XtLastTimestampProcessed(XtDisplay(w)),
		 convert_selection, lose_selection, NULL);
  owning = FALSE;
  return TRUE;
}

void 
HandleSelectStart( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  int r, c;

  DBUG_PRINT("events",("Event  SelectStart \n"));
  event_cell(w, event, &r, &c);
  fs_select_start(r, c);
  selecting = TRUE;
}

void 
HandleSelectExtend( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  int r, c;

  DBUG_PRINT("events",("Event  SelectExtend \n"));
  if (!selecting)
    return;
  event_cell(w, event, &r, &c);
  fs_select_extend(r, c);
}

void 
HandleSelectEnd( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  int r, c;

  DBUG_PRINT("events",("Event  SelectEnd \n"));
  if (!selecting)			/* e.g. the release of Ctrl+click */
    return;
  selecting = FALSE;
  event_cell(w, event, &r, &c);
  fs_select_extend(r, c);
  if (!own_selection(w, XA_PRIMARY, &primary_text))
    fs_cursor_to(r, c);			/* no drag: a click moves the cursor */
}

/* copy-selection(): offer the selected text as CLIPBOARD */
void 
HandleCopySelection( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  CopySelection \n"));
  own_selection(w, atom_named(w, "CLIPBOARD"), &clipboard_text);
}

/*
 * Paste: ask the first selection for UTF8_STRING, then STRING, then move
 * on to the next selection in the list, until one answers with text.
 * Requests are asynchronous; paste_req carries the state between them.
 */
static struct {
  Atom	sel[4];
  int	nsel;
  int	idx;			/* selection being asked */
  int	as_string;		/* asking for STRING (else UTF8_STRING) */
} paste_req;

static void request_paste ();

static void paste_received (w, client_data, selection, type, value,
			    length, format)
     Widget		w;
     XtPointer		client_data;
     Atom		*selection, *type;
     XtPointer		value;
     unsigned long	*length;
     int		*format;
{
  Atom utf8 = atom_named(w, "UTF8_STRING");

  if (value != NULL && *length > 0 && *format == 8
      && (*type == utf8 || *type == XA_STRING)) {
    unsigned char *text = (unsigned char *) value;
    int n = *length;

    if (*type == utf8) {		/* decode in place: never longer */
      n = utf8_to_latin1(text, n, text);
    }
    fs_paste(text, n);
    XtFree(value);
    return;
  }
  if (value != NULL)
    XtFree(value);
  /* nothing usable: next format, or next selection */
  if (!paste_req.as_string)
    paste_req.as_string = TRUE;
  else {
    paste_req.idx++;
    paste_req.as_string = FALSE;
  }
  request_paste(w);
}

static void request_paste (w)
     Widget	w;
{
  if (paste_req.idx >= paste_req.nsel)
    return;				/* no selection had text */
  XtGetSelectionValue(w, paste_req.sel[paste_req.idx],
		      paste_req.as_string ? XA_STRING
		      : atom_named(w, "UTF8_STRING"),
		      paste_received, NULL,
		      XtLastTimestampProcessed(XtDisplay(w)));
}

/* insert-selection(SEL, ...): paste the first of the named selections
 * (PRIMARY, SECONDARY, CLIPBOARD) that has text; default PRIMARY */
void 
HandleInsertSelection( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  int i;

  DBUG_PRINT("events",("Event  InsertSelection \n"));
  paste_req.nsel = 0;
  for (i = 0; i < *num_params && paste_req.nsel < 4; i++) {
    if (strcasecmp(params[i], "PRIMARY") == 0)
      paste_req.sel[paste_req.nsel++] = XA_PRIMARY;
    else if (strcasecmp(params[i], "SECONDARY") == 0)
      paste_req.sel[paste_req.nsel++] = XA_SECONDARY;
    else
      paste_req.sel[paste_req.nsel++] = atom_named(w, params[i]);
  }
  if (paste_req.nsel == 0)
    paste_req.sel[paste_req.nsel++] = XA_PRIMARY;
  paste_req.idx = 0;
  paste_req.as_string = FALSE;
  request_paste(w);
}

void 
HandleKeyboardSelectStart( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event KeyboardSelectStart \n"));
}

void 
HandleSelectSet( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event   SelectSet \n"));
}

void 
HandleKeyboardSelectEnd( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event KeyboardSelectEnd \n"));
}

void 
HandleStartExtend( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event \n"));
}

void 
HandleKeyboardStartExtend( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
 DBUG_PRINT("events",("Event  KeyboardStartExtend \n"));
}

void 
HandleBell( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event Bell \n"));
  
}

void 
HandleIgnore( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{ 
  DBUG_PRINT("events",("Event Ignore \n"));
}


void 
HandleCreateMenu( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{ 
  DBUG_PRINT("events",("Event CreateMenu \n"));
}

void 
HandlePopupMenu( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  PopupMenu \n"));
}

void 
HandleSetFont( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event SetFont\n"));
}

void 
HandleEnterNotify( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event HandleEnterNotify\n"));
  status_disp(10, CG_BLANK);
  status_disp(11, CG_BLANK);

}

void 
HandleLeaveNotify( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event LeaveNotify\n"));
  status_disp(10, CG_CLOCKLEFT);
  status_disp(11, CG_CLOCKRIGHT);
}

void 
HandleFocusIn( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event FocusIn\n"));
  status_disp (51, CG_INSERT);
}

void 
HandleFocusOut( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event FocusOut\n"));
  status_disp (51, CG_BLANK);
}

void 
HandleGraphicsExpose( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event HandleGraphicsExpose\n"));
}

void 
HandleExpose( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event HandleExpose width: %i  height: %i \n",
          event->xexpose.width, event->xexpose.height));

}

void 
HandleMapNotify( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event HandleMapNotify\n"));
}



void 
HandleRedraw( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event HandleRedraw\n"));
}



void 
HandleQuit( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event HandleQuit\n"));
}

extern int (*str_to_func())();

void
HandleKey( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  int (*func)();
  XButtonEvent *xb = (XButtonEvent *) event;
  DBUG_ENTER("HandleKey");

  DBUG_PRINT("key",("Event Key function=`%s'\n",params[0]));
/*  mettre a jour la position de la souris */

   xcurs = xb->x - (screen_width/2);
   ycurs = (screen_height/2) - xb->y;

  if ((func = str_to_func(params[0])) == NULL) 
	fprintf(stderr,"key function \"%s\"  not found\n", params[0]);
  else 
    (*func)();
  net_flush();

  DBUG_VOID_RETURN;
}

void 
HandleLocalKey( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{

  XKeyEvent *e = (XKeyEvent *) event;
  KeySym ks;
  int nbytes;
  char ch;

  DBUG_ENTER("HandleLocalKey");

  nbytes = XLookupString(e, &ch, 1, &ks, NULL);
  /* keys that produce no character (e.g. Command alone) used to send
   * whatever was in ch; Latin-1 keys must not go in sign-extended,
   * negative codes are the DUP / FieldMark pseudo keys */
  if (nbytes < 1)
    DBUG_VOID_RETURN;
  fs_character((unsigned char) ch);
  DBUG_PRINT("key",("key=\"%c\"\n",ch));
  DBUG_VOID_RETURN;
}

void 
HandleModifierKey( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event Modifier Key `%s' \n", params[0]));
}

extern        int    linelength;     /* from fs3270.c, linelength valid during this write */
void 
HandleMouseSetTextCursor( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  XButtonEvent *xb = (XButtonEvent *) event;
  int r,c;


  XawColScrXytoRc(screen_widget, xb->x,xb->y,&r,&c);
  DBUG_PRINT("events",("Event MouseSetTextCursor x=%d y=%d r=%d c=%d\n",xb->x,xb->y,r,c));

  cursor_move((r + 1) * linelength + c);

}




