
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


#include <X11/Xaw/Label.h>	
#include <X11/Xaw/Cardinals.h>	
#include <X11/Xaw/Box.h>	


#include "x3270.h"
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

void 
HandleInsertSelection( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  InsertSelection \n"));
}

void 
HandleSelectStart( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  SelectStart \n"));
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
HandleSelectExtend( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  SelectExtend \n"));
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
HandleSelectEnd( w, event, params, num_params)
     Widget w;
     XEvent *event;
     String *params;
     Cardinal *num_params;
{
  DBUG_PRINT("events",("Event  SelectEnd \n"));
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
  fs_character(ch);
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




