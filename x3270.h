
#define DEFAULT_TERM  "3"
#define ERROR_MSG_SIZE	200

typedef struct _AppResources {
  char *hostname;
  char *model;
  char *logfile;
  char *debug;
  int  portnumber;
  Pixel fg;			/* simple has no foreground */
  Pixel bg;
  Boolean synchronous;
} AppResources;

#define status_disp(x,y) XawCsModeSetChar(screen_widget,x,y)

/* eventhdlr.c */

extern void HandleModifierKey();
extern void HandleLocalKey();
extern void HandleKey();
extern void HandleMouseSetTextCursor();
extern void HandleKeyPressed();
extern void HandleStringEvent();
extern void HandleEnterNotify();
extern void HandleLeaveNotify();
extern void HandleFocusIn();
extern void HandleFocusOut();
extern void HandleExpose();
extern void HandleGraphicsExpose();
extern void HandleExposeModeLine();
extern void HandleRedraw();
extern void HandleQuit();

extern void HandleKeymapChange();
extern void HandleInsertSelection();
extern void HandleCopySelection();
extern void HandleSelectStart();
extern void HandleKeyboardSelectStart();
extern void HandleSelectExtend();
extern void HandleSelectSet();
extern void HandleSelectEnd();
extern void HandleKeyboardSelectEnd();
extern void HandleStartExtend();
extern void HandleKeyboardStartExtend();
extern void HandleBell();
extern void HandleIgnore();
extern void HandleSecure();
extern void HandleScrollForward();
extern void HandleScrollBack();
extern void HandleCreateMenu();
extern void HandlePopupMenu();
extern void HandleSetFont();
extern void HandleExposeModeLine();

/* modeline.c: */


extern void init_mode_line();
extern void redisplay_mode_line();


/* in x3270.c */

extern char	*prog_name;		/* argv[0] */
extern char	error_message[];	/* so we can display `error dialogs' */

extern AppResources app_resources;

extern Widget 	toplevel;
extern Widget 	form_widget;	/* contains actual 3270 screen, */
                                /* a horizontal line, and the status line */
extern Widget 	screen_widget;	/* actual text/graphics screen */


extern Display 	*dpy;		/* for raw xlib calls */
extern		XtAppContext app_con;



