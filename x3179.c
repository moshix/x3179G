
#include <stdio.h>
#include <stdlib.h>	/* atoi, exit: correct prototypes on 64-bit */
#include <assert.h>
#include <string.h>	/* strchr, strrchr: used by split_host_port */
#include <ctype.h>	/* isdigit */

#include <X11/Intrinsic.h>	
#include <X11/StringDefs.h>

#if  defined(XtSpecificationRelease) && (XtSpecificationRelease >= 4)
#include <X11/Xaw/Cardinals.h>	
#ifdef R4PULLDOWNS  
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Sme.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#endif
#include <X11/Xaw/Label.h>	
#include <X11/Xaw/Form.h>	
#else
#include <X11/Cardinals.h>	
#ifdef R4PULLDOWNS  
#include <X11/MenuButton.h>
#include <X11/SimpleMenu.h>
#include <X11/Sme.h>
#include <X11/SmeBSB.h>
#include <X11/SmeLine.h>
#endif
#include <X11/Label.h>	
#include <X11/Form.h>	
#endif
/*
 * Was "#define XtPointer int" (pre-R4 hack). On 64-bit hosts that
 * truncates pointers, e.g. (XtPointer)&app_resources below, so we
 * now use the real XtPointer (void *) from <X11/Intrinsic.h>.
 */
#include "ColorScreen.h"

#include "x3270.h"
#include "dbug.h"
#include "global.h"


#define MAXARGS 32
static int nargs;
static Arg wargs[MAXARGS];
#define startargs()		nargs = 0
#define setarg(name, value)	XtSetArg(wargs[nargs], name, value), nargs++
  


/* TEST TEST TEST */






extern Byte ebc2cg[];


/* TEST END */


  
  /* 
   * Structure for now:
   *  app_shell {
   *     form {           - in case someone wants to add a titlebar or such
   *	  titlebar {    - a form
   *            network   - pulldown
   *            options   - pulldown
   *        }
   *        colorscreen   - text area, a subclass of core
   *     }
   *  }
   */
  
  
  /*
   * These resources will only get loaded if there is no app-defaults
   * file for this application.  
   */
  String fallback_resources[] = { 
#if 0
    /* see X3270.appdefaults */
#endif
    NULL 
    };


static XtResource resources[] = {
  {"hostName", "HostName", XtRString, sizeof(String),
     XtOffset(AppResources *, hostname), XtRString, "wuw"},
  
  {"model", "Model", XtRString, sizeof(String),
     XtOffset(AppResources *, model),  XtRImmediate, (caddr_t) DEFAULT_TERM },
  
  {"logFile", "LogFile", XtRString, sizeof(String),
     XtOffset(AppResources *, logfile), XtRImmediate, NULL},
  
  {"debug", "Debug", XtRString, sizeof(String),
     XtOffset(AppResources *, debug), XtRImmediate, NULL},

  {"synchronous", "Synchronous", XtRBoolean, sizeof(Boolean),
     XtOffset(AppResources *, synchronous), XtRImmediate, (caddr_t) FALSE},
  
  {"port", "Port", XtRInt, sizeof(int),
     XtOffset(AppResources *, portnumber), XtRImmediate, 0},
  
  {"foreground", "Foreground", XtRPixel, sizeof(Pixel),
     XtOffset(AppResources *, fg), XtRString, (caddr_t) "SkyBlue"},
  
  {"background", "Background", XtRPixel, sizeof(Pixel),
     XtOffset(AppResources *, bg), XtRString, (caddr_t) "Black"},
};



static XrmOptionDescRec options[] = {
  {"-host",	"*hostName",	XrmoptionSepArg,	NULL},
  {"-model",	"*Model",	XrmoptionSepArg,	NULL},
  {"-logfile",  "*logFile",	XrmoptionSepArg,        NULL},
  {"-fg",  	"*foreground",	XrmoptionSepArg,        NULL},
  {"-bg",  	"*background",	XrmoptionSepArg,        NULL},
  {"-debug",  	"*debug",	XrmoptionSepArg,        NULL},
};


static XtActionsRec actions[] = {
  { "modifier-key",             HandleModifierKey },
  { "local-key",             	HandleLocalKey },
  { "key",                      HandleKey },
  { "mouse-set-text-cursor",    HandleMouseSetTextCursor },
  { "bell",             	HandleBell },
  { "create-menu",      	HandleCreateMenu },
  { "ignore",           	HandleIgnore },
  { "insert",           	HandleKeyPressed }, 
  { "insert-selection", 	HandleInsertSelection },
  { "keymap",           	HandleKeymapChange },
  { "popup-menu",       	HandlePopupMenu },
  { "select-start",     	HandleSelectStart },
  { "select-extend",    	HandleSelectExtend },
  { "select-end",       	HandleSelectEnd },
  { "select-set",       	HandleSelectSet },
  { "select-cursor-start",    HandleKeyboardSelectStart },
  { "select-cursor-end",      HandleKeyboardSelectEnd },
  { "start-extend",     	HandleStartExtend },
  { "start-cursor-extend",    HandleKeyboardStartExtend },
  { "string",           	HandleStringEvent },
  { "enter",           	HandleEnterNotify },
  { "leave",           	HandleLeaveNotify },
  { "focus-in",           	HandleFocusIn },
  { "focus-out",           	HandleFocusOut },
/*  { "expose",           	HandleExpose },  */
  { "graphics-expose",       	HandleGraphicsExpose },
  { "map",           		HandleKeymapChange },
  
  /* menu actions */
  
  { "redraw",                 HandleRedraw },
  { "quit",                   HandleQuit },
};

char 	*prog_name;
char 	error_message[ERROR_MSG_SIZE];
int     model_num;

AppResources 	app_resources;
Display 	*dpy;		/* needed for Xlib calls */

Widget 	toplevel;
Widget 	form_widget;		/* contains actual 3270 screen, */
Widget 	screen_widget;		/* actual text/graphics screen */

Widget 	title_widget;	        /* the titlebar box */
Widget  net_widget;		/* first titlebar pulldown button */
Widget  opt_widget;		/* second titlebar pulldown button */
Widget  net_pulldown_widget;	/* associated pulldowns */
Widget 	opt_pulldown_widget;
XtAppContext app_con;

static void set_size();


extern  int init();
extern  int next_item();


/*
 * X3179.ad, compiled in.  x3179_ad.h is generated from X3179.ad by
 * the Makefile and defines builtin_resources[] as one string.
 */
#include "x3179_ad.h"

/*
 * merge_builtin_resources - make X3179.ad the default without
 * needing XENVIRONMENT.
 *
 * Without the resource file the window came up white and the macOS
 * key bindings were missing.  We parse the built-in copy and merge it
 * into the display's database with override=False, so it acts like an
 * app-defaults file: an entry already there (from XENVIRONMENT,
 * ~/.Xdefaults, xrdb, -xrm or -bg/-fg) is kept, ours only fills gaps.
 * XrmCombineDatabase consumes builtin, so it is not destroyed here.
 */
static void
merge_builtin_resources(dpy)
     Display *dpy;
{
  XrmDatabase builtin, current;

  builtin = XrmGetStringDatabase(builtin_resources);
  if (builtin == NULL)
    return;
  current = XtDatabase(dpy);
  XrmCombineDatabase(builtin, &current, False);
}

/*
 * split_host_port - let "-host name:port" carry the port as well.
 *
 * If app_resources.hostname looks like "name:digits", the hostname is
 * cut at the colon and the digits become app_resources.portnumber,
 * overriding any *port resource (the command line is more explicit).
 * A name with no colon is left alone, so "-host name" plus
 * "-xrm '*port: N'" keeps working as before.
 *
 * Only a single colon is accepted: anything with more than one (an
 * IPv6 literal) is not split, and gethostbyname() would reject it
 * anyway.  A bad port ("name:", "name:abc", "name:0", "name:70000")
 * is a fatal error rather than a silent fallback to the telnet port.
 */
static void
split_host_port()
{
  char *name, *colon, *p;
  long port;

  if (app_resources.hostname == NULL)
    return;

  colon = strchr(app_resources.hostname, ':');
  if (colon == NULL || strrchr(app_resources.hostname, ':') != colon)
    return;

  /* port part must be all digits and in 1..65535 */
  for (p = colon + 1; *p; p++)
    if (!isdigit((unsigned char) *p))
      break;
  port = (*p == '\0' && p != colon + 1) ? atol(colon + 1) : 0;
  if (port < 1 || port > 65535)
    {
      fprintf(stderr, "%s: bad port in -host \"%s\"\n",
	      prog_name, app_resources.hostname);
      exit(1);
    }

  /*
   * The resource string belongs to the Xrm database, so copy it
   * before cutting it at the colon.  The copy is never freed: it
   * lives as long as app_resources does.
   */
  name = XtNewString(app_resources.hostname);
  name[colon - app_resources.hostname] = '\0';
  app_resources.hostname = name;
  app_resources.portnumber = (int) port;
}


main(argc, argv)
     int argc;
     char **argv;
{
  void fill();
  int rows, cols;
  char buf[100];

  DBUG_ENTER("main");

  prog_name = argv[0];

  DBUG_PROCESS(prog_name);

  /*
   * Initialize the Toolkit, set the fallback resources, and get
   * the application context associated with this application.
   */
  
/*toplevel = XtAppInitialize( &app_con, "X3270", options, XtNumber(options),
			     &argc, argv,
			     fallback_resources, NULL, ZERO);  */

toplevel = XtInitialize(NULL,"X3270", options, XtNumber(options),
	  &argc, argv);
      app_con = XtWidgetToApplicationContext(toplevel);

  merge_builtin_resources(XtDisplay(toplevel));

  /*
   * Add string to function bindings for out application actions.
   */
  
   XtAppAddActions(app_con, actions, XtNumber(actions));
  
  /*
   * Actually load the application resources, and store them in the
   * structure pointed to by &app_resources.
   */
  
  XtGetApplicationResources( toplevel, (XtPointer) &app_resources,
			    resources, XtNumber(resources), NULL, ZERO );

  split_host_port();		/* "-host name:port" */

  if (app_resources.debug != NULL) 
    {
      printf("debug option : \"%s\"\n",app_resources.debug);
      DBUG_PUSH(app_resources.debug);
    }

  create_widgets();
  
  /*
   * Create the windows, and set their attributes according
   * to the Widget data.
   */

   XtRealizeWidget(toplevel);
  dpy = XtDisplay( toplevel );

  XSynchronize(dpy, app_resources.synchronous);

    switch (atoi(app_resources.model)) {
	case 2:
	    rows = 24; cols = 80;
	    model_num = 2;
	    break;
	case 3:
	    rows = 32; cols = 80;
	    model_num = 3;
	    break;
	case 4:
	    rows = 43; cols = 80;
	    model_num = 4;
	    break;
	case 5:
	    rows = 27; cols = 132;
	    model_num = 5;
	    break;
	default:
	    {
		sprintf(buf, "unknown model %s, defaulting to 4", 
			app_resources.model);
	        XtWarning(buf);
	        rows = 43; cols = 80;
	        model_num = 4;
	    }
	    break;
    }
  set_size(rows,cols);

#ifndef R4PULLDOWNS
  connect_to_host(app_resources.hostname,app_resources.portnumber);
#endif
  /*
   * Now process the events.
   */
  
  XtAppMainLoop(app_con);
}



char *net_menu_names[] = {
  "-",
  "Connect to..",
  "Disconnect",
  "-",
  "Quit",
  NULL
  };

char *opt_menu_names[] = {
  "-",
  "24 x 80 Model 2",
  "32 x 80 Model 3",
  "43 x 80 Model 4",
  "27 x 132 Model 5",
  "Toggle Base Color",
  "-",
  "Debug",
  NULL
  };


static void
  net_menu_callback(w, item, call_data)
Widget w;
int item;
XtPointer call_data;
{
  switch (item) {
  case 1:
    connect_to_host(app_resources.hostname,app_resources.portnumber);
    break;
  case 2:
    disconnect_from_host();
    break;
  case 4:
    exit(0);
  }
}

static void
set_size(lines,cols)
     int lines, cols;
{
  fs_set_screensize(lines,cols);
  startargs();
  setarg(XtNlines,lines);
  setarg(XtNcolumns,cols);
  XtSetValues(screen_widget,wargs,nargs);
}

static void
opt_menu_callback(w, item, call_data)
     Widget w;
     int item;
     XtPointer call_data;
{
  
  Dimension width,height;
  int lines, cols;

  XawColScrSize(screen_widget, &lines, &cols);
  
  printf("options mxenu: item %d selected\n",item);
   switch (item) {
  case 1:    lines = 24; cols = 80; model_num = 2; break;
  case 2:    lines = 32; cols = 80; model_num = 3; break;
  case 3:    lines = 43; cols = 80; model_num = 4; break;
  case 4:    lines = 27; cols = 132; model_num = 5; break;
  case 5:    fs_toggle_basecolor(); return;
  case 6:    /*debug++; */  break;
  }

  set_size(lines,cols);
}

/*  switch (item) {
  case 1:    lines = 24; break;
  case 2:    lines = 32; break;
  case 3:    cols = 80; break;
  case 4:    cols = 132; break;
  case 5:    fs_toggle_basecolor(); return;
  case 6:    debug++;  break;
  }

  set_size(lines,cols);
}

*/



create_widgets()
{
  int cols, rows;

  form_widget = XtCreateManagedWidget( "main", formWidgetClass, toplevel, 
				      NULL, 0);
  
#ifdef R4PULLDOWNS  
  title_widget = XtCreateManagedWidget("titlebar", formWidgetClass,
				       form_widget, NULL, 0);
#endif  
  screen_widget = XtCreateManagedWidget( "screen", colorScreenWidgetClass, 
					form_widget, 
					NULL, 0);
#ifdef R4PULLDOWNS  
  net_widget = XtCreateManagedWidget("net", menuButtonWidgetClass,
				     title_widget,  NULL, ZERO);
  
  
  opt_widget = XtCreateManagedWidget("options", menuButtonWidgetClass,
				     title_widget,  NULL, ZERO);
  
  net_pulldown_widget = XtCreatePopupShell("netmenu", simpleMenuWidgetClass,
					   net_widget, NULL, ZERO);
  
  opt_pulldown_widget = XtCreatePopupShell("optmenu", simpleMenuWidgetClass,
					   opt_widget, NULL, ZERO);
  
  create_pd(net_pulldown_widget,"net",net_menu_names,net_menu_callback);
  
  create_pd(opt_pulldown_widget,"opt",opt_menu_names,opt_menu_callback);
#endif  
  XawColScrSet(screen_widget, init, next_item);
 
  XawColScrSize(screen_widget, &rows, &cols);

				
  fs_set_screensize(rows,cols);     /* tell emulator about screensize */
}

#ifdef R4PULLDOWNS  
create_pd(w, prefix, names, callback)
     Widget w;
     char *prefix;
     char **names;
     XtCallbackProc callback;
{
  char name[100];
  char **s = names;
  int i = 0;
  Widget entry;
  
  while (*s != NULL) 
    {
      sprintf(name,"%s-entry-%d",prefix,i);
      switch (**s) {
	
        /*
         * Create a blank entry.
         */
      case '\0' :
	entry = XtCreateWidget(name, smeObjectClass, w, NULL, ZERO);
	break;
	
        /*
         * Create a separator line.
         */
      case '-':
	entry = XtCreateManagedWidget(name, smeLineObjectClass, w,
				      NULL, ZERO);
	break;
	
	/* A real entry. */
      default:
	entry = XtCreateManagedWidget(*s, smeBSBObjectClass,w,
                                      NULL, ZERO);
        XtAddCallback(entry, XtNcallback, callback, i);
	break;
      }
      s++;i++;
    }
}
#endif

Bell()
{
   XBell(XtDisplay(screen_widget),0);
}


























