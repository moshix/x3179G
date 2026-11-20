/*
 * 	ColorScreenP.h
 *		based on R4 Xaw Label
 *		Fri Jun  1 19:22:47 1990 -- Michael Haberler
 */

/* 
 * ColorScreenP.h - Private definitions for ColorScreen widget
 * 
 */

#ifndef _XawColorScreenP_h
#define _XawColorScreenP_h

/***********************************************************************
 *
 * ColorScreen Widget Private Data
 *
 ***********************************************************************/
#if 1
#include "ColorScreen.h"
#else
#include <X11/Xaw/ColorScreen.h>
#endif

#include "global.h"


/* New fields for the ColorScreen widget class record */

typedef struct {int foo;} ColorScreenClassPart;

/* Full class record declaration */
typedef struct _ColorScreenClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
    ColorScreenClassPart colorScreen_class;
} ColorScreenClassRec;

extern ColorScreenClassRec colorScreenClassRec;

                                                                          
/* New fields for the ColorScreen widget record */
typedef struct {

    /* resources */

    Cardinal	lines;			/* usually 24 or 32 */
    Cardinal	columns;		/* usually 80 or 132 */
    Dimension   line_spacing;		/* in the text area */
    Pixel	colors[N_COLORS];	/* foreground = default = colors[0] = green, usually */
    /* see global.h for color indices */
    
    XFontStruct	*fonts[N_FONTS];	/* actually just 2 or 3 - default, apl and maybe big */

    Boolean 	block_cursor;	
    Boolean 	blinking_cursor;	
    int         blink_duration;

    Dimension	internal_width;		/* same semantis as Label */
    Dimension	internal_height;
    unsigned char	default_char; 	/* not all fonts have space in the same position */
    Dimension	sep_line_width;		/* width of separator line */
    Dimension	mode_internal_width;	/* same semantis as Label */
    Dimension	mode_internal_height; 	/* Modeline only */
    Boolean     back_pixmap;            /* store graphic in backup pixmap */    
    int         thick_line;             /* number of Pixels for drawing a thick line */
    String      scalable_font;          /* XLFD with one %d (pixel size), "" = off */
    int         font_size;              /* initial pixel size for scalable_font */

    /* private state */

    int 	current_font;		/* index into font[] */
    GC		text_gc;	 	/* nonshared */
    GC		graph_gc;
    GC          cursor_gc;
    GC          mode_gc;

#define CURSOR_VISIBLE            0x01	/* cursor is turned on, maybe not displayed */
#define CURSOR_DISPLAYED          0x02	/* cursor is actually visible */
#define CURSOR_BLINKING_BLOCKED   0x04	/* during some operations, we  better dont 
					 * blink the cursor */
    unsigned int cursor_state;
    Pixel cursor_color;
    XtIntervalId blink_interval_id;

    int         cursor_x;
    int         cursor_y;
    unsigned char cursor_ch;	        /* character under cursor */
    unsigned long cursor_attr;	        /* attribute of character under cursor */



#if 0
    unsigned char 	*screen;	/* contents of screen buffer */
    unsigned long	*att;		/* corresponding attributes for screen 
					   buffer */
#endif
    Dimension	char_width, char_height;
    int         cur_font_px;            /* pixel size in use; 0 = font not scalable */
    Dimension   x_margin;               /* left edge of text grid (grid is centred */
    Dimension   y_margin;               /* top edge   in a window larger than it)  */
    unsigned    char 	*modeline;	/* contents of status area */
    Position 	mode_x, mode_y;		/* position of modeline string */
    Position 	sep_line_y;		/* y offset of separator line */
    Pixmap              bpixmap;        /* backup pixmap */
    int         (*init)();              /* function: init region to redraw
					   ---\ set by  */
    int         (*next_field)();        /* function: get next field in region  
					   ---/ XawColScrSet() */

    /* 3270 graphics state */

} ColorScreenPart;


/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _ColorScreenRec {
    CorePart		core;
    SimplePart		simple;
    ColorScreenPart	colorScreen;
} ColorScreenRec;

#endif /* _XawColorScreenP_h */






































