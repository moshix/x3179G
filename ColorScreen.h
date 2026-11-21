/*
 * ColorScreen.h
 *
 *	All what's needed for an IBM3x79 color graphics terminal
 *	emulator
 *
 *	lifted from Xaw Label widget
 *	Fri Jun  1 19:21:08 1990 -- Michael Haberler
 */

#ifndef _XawColorScreen_h
#define _XawColorScreen_h

/***********************************************************************
 *
 * ColorScreen Widget
 *
 ***********************************************************************/
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#if  defined(XtSpecificationRelease) && (XtSpecificationRelease >= 4)
#include <X11/Xaw/XawInit.h>
#include <X11/Xaw/SimpleP.h>
#include <X11/Xaw/Simple.h>
#include <X11/Xmu/Converters.h>
#else
#include <X11/SimpleP.h>
#include <X11/Simple.h>
#endif


/* Resources:

 Name		     Class		RepType		Default Value		ColorScreen Vs.Label
 ----		     -----		-------		-------------
 background	     Background		Pixel		black			ok
 border		     BorderColor	Pixel		XtDefaultForeground	ok
 borderWidth	     BorderWidth	Dimension	1			ok
 cursor		     Cursor		Cursor		None			Light Pen?
 destroyCallback     Callback		XtCallbackList	NULL		        ok
 font		     Font		XFontStruct*	XtDefaultFont		ok
 foreground	     Foreground		Pixel		XtDefaultForeground	ok
 insensitiveBorder   Insensitive	Pixmap		Gray			ok - not connected?
 mappedWhenManaged   MappedWhenManaged	Boolean		True			ok
 resize		     Resize		Boolean		True			ok
 sensitive	     Sensitive		Boolean		True			ok
 x		     Position		Position	0			ok
 y		     Position		Position	0			ok


 new to ColorScreen:
 -------------------
										Widget Record field:
 lines		     Lines		Cardinal	24			lines
 colums		     Columns		Cardinal	80			columns
 font	     	     Font		XFontStruct *	ibm-3270		fonts[0]
 apl_font	     AplFont		XFontStruct *   ibm-3270-apl		fonts[1]
 modeline_font	     ModeLineFont	XFontStruct *	ibm-3270		fonts[2]
 linespacing         Linespacing        Dimension       2                       line_spacing
 ibm_default	     IBM_default	Pixel		green			colors[0]
 ibm_blue	     IBM_blue		Pixel		DeepSkyBlue		colors[1]
 ibm_red	     IBM_red		Pixel		OrangeRed		colors[2]
 ibm_pink	     IBM_pink		Pixel		pink			colors[3]
 ibm_green	     IBM_green		Pixel		green			colors[4]
 ibm_turquoise	     IBM_turquoise	Pixel		turquoise		colors[5]
 ibm_yellow	     IBM_yellow		Pixel		yellow			colors[6]
 ibm_white	     IBM_white		Pixel		white			colors[7]
 ibm_black           IBM_black          Pixel           black                   colors[8]  
 cursorshape	     Cursorshape	Cardinal	0			cursor_shape, block vs. line
 
 internalWidth	     InternalWidth      Dimension       4
 internalHeight	     InternalHeight	Dimension	2
 defaultChar	     DefaultChar	unsigned char	16			font dependent blank
 sepLineWidth	     SepLineWidth	Dimension	2
 modeInternalHeight  ModeInternalHeight Dimension       2
 modeInternalWidth   ModeInternalWidth  Dimension       2
 backPixmap          BackPixmap         Boolean         FALSE
 thickLine           ThickLine          Cardinal        3

*/

# define XtNlines 		"lines"
# define XtCLines 		"Lines"
# define XtNcolumns 		"colums"
# define XtCColumns 		"Columns"
# define XtNapl_font 		"apl_font"
# define XtCAplFont 		"AplFont"

# define XtNmodeline_font 	"modeline_font"
# define XtCModeLineFont 	"ModeLineFont"

# define XtNlinespacing 	"linespacing"
# define XtCLinespacing 	"Linespacing"
# define XtNibm_default 	"ibm_default"
# define XtCIBM_default 	"IBM_default"
# define XtNibm_blue 		"ibm_blue"
# define XtCIBM_blue 		"IBM_blue"
# define XtNibm_red 		"ibm_red"
# define XtCIBM_red 		"IBM_red"
# define XtNibm_pink 		"ibm_pink"
# define XtCIBM_pink 		"IBM_pink"
# define XtNibm_green 		"ibm_green"
# define XtCIBM_green 		"IBM_green"
# define XtNibm_turquoise 	"ibm_turquoise"
# define XtCIBM_turquoise 	"IBM_turquoise"
# define XtNibm_yellow 		"ibm_yellow"
# define XtCIBM_yellow 		"IBM_yellow"
# define XtNibm_white 		"ibm_white"
# define XtCIBM_white 		"IBM_white"
# define XtNibm_black 		"ibm_black"
# define XtCIBM_black 		"IBM_black"
# define XtNcursorshape 	"cursorshape"
# define XtCCursorshape 	"Cursorshape"
# define XtNdefaultchar 	"defaultchar"
# define XtCDefaultchar 	"Defaultchar"
# define XtNsepLineWidth	"sepLineWidth"
# define XtCSepLineWidth	"SepLineWidth"
# define XtNmodeInternalHeight	"modeInternalHeight"
# define XtCModeInternalHeight	"ModeInternalHeight"
# define XtNmodeInternalWidth	"modeInternalWidth"
# define XtCModeInternalWidth	"ModeInternalWidth"
# define XtNbackPixmap          "backPixmap"
# define XtCBackPixmap          "BackPixmap" 
# define XtNthickLine           "thickLine"
# define XtCThickLine           "ThickLine"
# define XtNBlockCursor         "blockcursor"
# define XtCBlockCursor         "BlockCursor"
# define XtNBlinkingCursor      "blinkingcursor"
# define XtCBlinkingCursor      "BlinkingCursor"
# define XtNBlinkDuration       "blinkduration"
# define XtCBlinkDuration       "BlinkDuration"
/* scalable text: XLFD pattern with one %d for the pixel size */
# define XtNscalableFont        "scalableFont"
# define XtCScalableFont        "ScalableFont"
# define XtNfontSize            "fontSize"
# define XtCFontSize            "FontSize"


# define N_FONTS	3
# define DEFAULT_FONT 	0
# define APL_FONT 	1
# define MODE_FONT	2

# define N_COLORS	9

/*
# define MAX_SCREEN_SIZE	5000
*/
# define MAX_COLUMNS	200



/* public functions */

extern void XawColScrCursorToggle();
extern void XawColScrCursorMove(/*w,x,y,style,attribute,char[x,y]*/);
extern void XawDrawLine();
extern void XawDrawLinesR();
extern void XawColScrDrawString();
extern void XawColScrRefresh();
extern void XawSetGraphColor();
extern void XawFillArea();
extern void XawFillAreaR();
extern void XawSetLineType();
extern void XawSetLineWidth();
extern void XawSetForegroundMix();
extern void XawSetPatternON();
extern void XawSetPatternOFF();
extern void XawDrawString();
extern void XawDrawImage();

/* extern void XawErase(); */

extern void XawColScrSet();
extern void XawColScrSize();
extern void XawColScrXyToCell();	/* pixel -> screen cell (row, column) */

/* Class record constants */

extern WidgetClass colorScreenWidgetClass;

typedef struct _ColorScreenClassRec *ColorScreenWidgetClass;
typedef struct _ColorScreenRec      *ColorScreenWidget;

#endif /* _XawColorScreen_h */
/* DON'T ADD STUFF AFTER THIS #endif */















