# include <stdio.h>
# include <stdlib.h>	/* malloc/realloc/free prototypes */
# include "global.h"
# include "dbug.h"
# include "ibm3270.h"
# include <X11/Xlib.h>
# include <X11/IntrinsicP.h>
# include <X11/StringDefs.h>
# include "ColorScreenP.h"

#define DrawLinesR(p, n)         XawDrawLinesR(screen_widget, p, n)
#define DrawLines(p, n)          XawDrawLines(screen_widget, p, n)
#define SetGraphColor(c)         XawSetGraphColor(screen_widget, c)
#define FillArea(p, n)           XawFillArea(screen_widget, p, n)
#define FillAreaR(p, n)          XawFillAreaR(screen_widget, p, n)
#define SetLineType(t)           XawSetLineType(screen_widget, t)
#define SetLineWidth(t)          XawSetLineWidth(screen_widget, t)
#define SetForegroundMix(m)      XawSetForegroundMix(screen_widget, m)
#define SetPatternSym(s)         XawSetPatternSym(screen_widget, s)       
#define SetPatternON()           XawSetPatternON(screen_widget)
#define SetPatternOFF()          XawSetPatternOFF(screen_widget)
#define DrawString(x, y, s, l)   XawDrawString(screen_widget, x, y, s, l)
#define DrawImage(id,ic,IX,IY,IW,IH) XawDrawImage(screen_widget,id,ic,IX,IY,IW,IH);


extern ColorScreenWidget screen_widget;


 /* Procedural Instructions, sect.3.3.3 
  * 
  */

#define AREASIZE 1024
#define IMAGESIZE 1024

static XPoint *area_points;
static int area_count=0;

static Boolean area_Flag=FALSE;
static Boolean area_Boundary=FALSE;

static Byte *image_data;
static Boolean image_Flag=FALSE;
static int image_count=0, imult=1; 

static Boolean pattern_Flag=FALSE;

static int X, Y, Img_X, Img_Y, Img_Width, Img_Height;
/*  static double aspect=(4.0/3.0); */
  static double aspect=1.0;


#define SETX(x) ((x) + (screen_width/2))
#define SETY(y) (((screen_height/2) - (short)(y)))
#define CORR(y) ((int)((y)*aspect+0.5))

int
em_erase_graphics_presentation_space()
{
	DBUG_ENTER("em_erase_graphics_presentation_space");

	clear_window();

	DBUG_VOID_RETURN;
}


int
em_stop_draw()
{
	DBUG_ENTER("em_stop_draw");

	DBUG_VOID_RETURN;
}


int
em_attach_graphic_cursor(buf,len)
{
	DBUG_ENTER("em_attach_graphic_cursor");
     set_cross_cursor(screen_widget);
	DBUG_VOID_RETURN;
}


int
em_detach_graphic_cursor()
{
	DBUG_ENTER("em_detach_graphic_cursor");
     set_arrow_cursor(screen_widget);
	DBUG_VOID_RETURN;
}


int
em_set_graphic_cursor_position(buf,len)
	Byte *buf;
	int len;

{
	DBUG_ENTER("em_set_graphic_cursor_position");
	DBUG_VOID_RETURN;
}


int
em_set_current_defaults(buf,len)
	Byte *buf;
	int len;
{
        char setdflt;
	int i;
  
	DBUG_ENTER("em_set_current_defaults");
  
	setdflt = ((buf[5] & 0x80) == 0);
	DBUG_PRINT("line",("def: setdef: %i", setdflt));
	DBUG_PRINT("line",("def: buf[2]: %x",buf[2]));
	switch(buf[2]){
		case DEF_DRAW_ATTR:
	             i = 6;
		     DBUG_PRINT("line",(" buf[3]: %x ", buf[3]));
		     if (buf[3] & 0x80){ /* color specified */
		        if (setdflt){
			   SetGraphColor(0); /* green */
			   DBUG_PRINT("line",("default color  \n"));
			}			
			else{
			   SetGraphColor(buf[i + 1]);
			   DBUG_PRINT("line",("default: color:  \n", buf[i + 1]));	
			}
			i += 2;
		     }
		     if (buf[3] & 0x20){ /* foreground mix specified */
		        if (setdflt){
			  DBUG_PRINT("line",("default: forg mix: %i \n", 0)); 
			  SetForegroundMix(0);  /* overpaint */
			}
			else{
			  SetForegroundMix(buf[i]);
			  DBUG_PRINT("line",("default: forg mix: %i \n", buf[i + 1])); 
			}
			i++;
		     }
		     break;
		case DEF_LINE_ATTR:
		     i = 6;
		     if (buf[3] & 0x80){ /* line type specified */
		        if (setdflt){
			   SetLineType(0); /* solid line */
			}
			else{
			   SetLineType(buf[i]);
			}
			i++;
		     }
		     if (buf[3] & 0x40){ /* line width specified */
		        if (setdflt){
			   SetLineWidth(0);
			}
			else{
			   SetLineWidth(buf[i]);
			}
		     }
		     break;
		case DEF_CHAR_ATTR:
		     break; /* character attributes */
		case DEF_MARKER_ATTR:
		     break; /* missing !!!!!!!!!!!!! */		     
		case DEF_PATTERN_ATTR:
		     /*
		      * i was never set on this path (it read buf[garbage]).
		      * Values start at offset 6, as in the cases above.
		      */
		     i = 6;
		     if (buf[3] & 0x01){ /* pattern symbol */
		        if (setdflt){
			   SetPatternSym(0);
			   pattern_Flag = FALSE;
			   DBUG_PRINT("line",("default pattern  \n"));
			}
			else{
			  SetPatternSym(buf[i]);
			  pattern_Flag = TRUE;
			  DBUG_PRINT("line",("default: pattern:\n", buf[i]));
			}
	             }		
		     break;
		case DEF_ARC_PARAM:
		     break; /* missing !!!!!!!!!!!!! */ 
		default:
		     break;
        }	
	DBUG_VOID_RETURN;
}


int
em_proc_noop()
{
	DBUG_ENTER("em_proc_noop");
	DBUG_VOID_RETURN;
}


int
em_proc_comment(buf,len)
	Byte *buf;
	int len;
{
	DBUG_ENTER("em_proc_comment");
	DBUG_VOID_RETURN;
}




 /* Drawing Orders */


int
em_begin_area(buf)
	Byte *buf;
{
	int act;

	DBUG_ENTER("em_begin_area");
	area_Flag = TRUE;
	area_count = 0;
	area_points = malloc(AREASIZE * sizeof(XPoint));
        DBUG_PRINT("line",("Boundary Line : %i\n", buf[1] & 0x40));
        area_Boundary = buf[1] & 0x40; /* Bit1: Flag: 1 if boundary lines are to be drawn, 0 if not */
	DBUG_RETURN(act);
}


int
em_begin_image(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_begin_image");
	
	image_Flag = TRUE;
	image_count = 0;
	imult = 1;
	image_data = malloc(IMAGESIZE * sizeof(Byte));

	Img_X =	SETX(FIELDLEN(buf + 2));
	Img_Y = CORR(SETY(FIELDLEN(buf + 4)));
 
	Img_Width = FIELDLEN(buf + 8);
        Img_Height = FIELDLEN(buf + 10);

	DBUG_PRINT("line",("\nImage: Width: %i, Height: %i", Img_Width, Img_Height));

	DBUG_RETURN(act);
}


int
em_begin_image_cp(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_begin_image_cp");

	image_Flag = TRUE;
	image_count = 0;
	imult = 1;
	image_data = malloc(IMAGESIZE * sizeof(Byte));

	Img_X =	X;
	Img_Y = CORR(Y);
 
	Img_Width = FIELDLEN(buf + 4);
        Img_Height = FIELDLEN(buf + 6);

	DBUG_PRINT("line",("Image: Width: %i, Height: %i", Img_Width, Img_Height));

	DBUG_RETURN(act);
}


int
em_character_string(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_character_string");
	
	X = SETX(FIELDLEN(buf + 2));
	Y = SETY(FIELDLEN(buf + 4));
      
        DrawString(X, CORR(Y), buf + 6, buf[1] - 4);
	
	DBUG_RETURN(act);
}


int
em_character_string_cp(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_character_string_cp");

        DrawString(X, CORR(Y), buf + 2, buf[1]);
        
	DBUG_RETURN(act);
}


int
em_end_area(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_end_area");
        if (area_Flag && area_count){
	   FillArea(area_points, area_count);
	   free(area_points);
	   area_count = 0;
	}   
  	area_Flag = FALSE;
	area_Boundary = FALSE;
	DBUG_RETURN(act);
}


int
em_end_image(buf,len)
	Byte *buf;
	int len;
{
	int act, i;

	DBUG_ENTER("em_end_image");

	if (image_Flag && image_count){
	   DrawImage(image_data, image_count, Img_X, Img_Y, Img_Width, Img_Height);
	   free(image_data);
	   image_count = 0;

	}
	imult = 1;
	image_Flag = FALSE;
	DBUG_RETURN(act);
}


int
em_fillet(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_fillet");

	DBUG_RETURN(act);
}


int
em_fillet_cp(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_fillet_cp");

	DBUG_RETURN(act);
}


int
em_full_arc(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_full_arc");

	DBUG_RETURN(act);
}


int
em_full_arc_cp(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_full_arc_cp");

	DBUG_RETURN(act);
}


int
em_image_data(buf,len)
	Byte *buf;
	int len;
{
       
	int act;
	int i, n;

	Byte *new_data, data;
	
	DBUG_ENTER("em_image_data");
	
	n = buf[1];

	if ((image_count + n) > (IMAGESIZE * imult)){
	   imult ++;
	   new_data = realloc(image_data, imult * IMAGESIZE * sizeof(Byte));
	   image_data = new_data;
	}
	bcopy(buf + 2, image_data + image_count, n); 
	
#if 0
	for (i = 2; i < n + 2; i++){
	    image_data[i+image_count-2] = buf[i]; /* FIXME  ugly! */
	}
#endif
	image_count += n;
	
	DBUG_RETURN(act);
}


int
em_line(buf,len)
	Byte *buf;
	int len;
{
	int act,i, fy, n=0;
	XPoint points[257];

	DBUG_ENTER("em_line");
	for (i= 2; i < 2 + buf[1]; i+= 4, n++) {
	  points[n].x = SETX(FIELDLEN(buf + i));
	  points[n].y = CORR(SETY(FIELDLEN(buf + i + 2)));
        }
	fy = SETY(FIELDLEN(buf + 4));
	X = SETX(FIELDLEN(buf + buf[1] - 2));
	Y = SETY(FIELDLEN(buf + buf[1]));
	if (area_Flag){
	  PrepareFillArea(points, fy, Y, n);
	  if (area_Boundary){
	    if (pattern_Flag){
	       SetPatternOFF();
	       DrawLines(points, n);
               SetPatternON();
	    }else{
	      DrawLines(points, n);
	    }
	  }
	}
	else{
	  if (pattern_Flag){
	      SetPatternOFF();
	      DrawLines(points, n);
              SetPatternON();
	  }
	  else{
	     DrawLines(points, n);
	  }
	}  
	DBUG_RETURN(act);
}


int
em_line_cp(buf,len)
	Byte *buf;
	int len;
{
	int act,i, fy, n=1;
	XPoint points[257];

	DBUG_ENTER("em_line_cp");
	points[0].x = X;
	points[0].y = CORR(Y);
        fy = Y;
	for (i= 2; i < 2 + buf[1]; i+= 4, n++) {
	  points[n].x = SETX(FIELDLEN(buf + i));
	  points[n].y = CORR(SETY(FIELDLEN(buf + i + 2)));
}
	X = SETX(FIELDLEN(buf + buf[1] - 2));
	Y = SETY(FIELDLEN(buf + buf[1]));
	if (area_Flag){
	  PrepareFillArea(points, fy, Y, n);
	  if (area_Boundary){
	     if (pattern_Flag){
	         SetPatternOFF();
		 DrawLines(points, n);
		 SetPatternON();
	     }
	     else{
	       DrawLines(points, n);
	     }
	   }
	}
	else{
	  if (pattern_Flag){
	      SetPatternOFF();
	      DrawLines(points, n);
              SetPatternON();
	  }
	  else{
	     DrawLines(points, n);
	  }
	}  
	DBUG_RETURN(act);
}


int
em_marker(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_marker");

	DBUG_RETURN(act);
}


int
em_marker_cp(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_marker_cp");

	DBUG_RETURN(act);
}


int
em_relative_line(buf,len)
	Byte *buf;
	int len;
{
	int bx,by;
        int act, i, fy, n=1;
        int cur_x, cur_y;
	XPoint points[257];

	DBUG_ENTER("em_relative_line");

	cur_x = SETX(FIELDLEN(buf + 2));
	fy = cur_y = SETY(FIELDLEN(buf + 4));
	points[0].x = cur_x;
	points[0].y = CORR(cur_y);
	DBUG_PRINT("line",("X0=%d Y0=%d len=%d\n",points[0].x, points[0].y, buf[1]));
        for (i = 6; i < 2 + buf[1]; i += 2, n++){
	  bx = *((Byte *)(buf + i));
	  by = (*((Byte *)(buf + i + 1)));
	  if (bx > 127) bx = bx - 256 ;
	  if (by > 127) by = by - 256 ;
	  cur_x += bx;
	  cur_y -= by;
 /* XWindows: CoordModePrevious -> +dx, | +dy */
 /*                                     v     */
 	  points[n].x = cur_x;
	  points[n].y = CORR(cur_y);  
	  DBUG_PRINT("line",("PointX=%d PointY=%d\n",points[n].x,points[n].y));
        } 
	DBUG_PRINT("line",("n=%d\n",n));
	X = cur_x;
	Y = cur_y;
	if (area_Flag){
	  PrepareFillArea(points, fy, Y, n);
	  if (area_Boundary){
	     if (pattern_Flag){
	         SetPatternOFF();
		 DrawLines(points, n);
		 SetPatternON();
	     }
	     else{
	       DrawLines(points, n);
	     }
	  }
	}
	else{
	  if (pattern_Flag){
	      SetPatternOFF();
	      DrawLines(points, n);
              SetPatternON();
	  }
	  else{
	    DrawLines(points, n);
	    }
	}  
	DBUG_RETURN(act);
}


int
em_relative_line_cp(buf,len)
	Byte *buf;
	int len;
{
	int bx,by;
	int act,i, fy, n=1;
        int cur_x, cur_y;
	XPoint points[257];

	DBUG_ENTER("em_relative_line_cp");

        cur_x = X;
        fy = cur_y = Y;
	points[0].x = cur_x;
	points[0].y = CORR(cur_y);
	DBUG_PRINT("line",("X0=%d Y0=%d\n",X,Y));
	for (i = 2; i < 2 + buf[1]; i += 2, n++) {
	  bx = *((Byte *)(buf + i));
	  by = (*((Byte *)(buf + i + 1)));
	  if (bx > 127) bx = bx - 256 ;
	  if (by > 127) by = by - 256 ;
	  cur_x += bx;
	  cur_y -= by;
  /* XWindows: CoordModePrevious -> +dx, | +dy */
  /*                                     v     */
	  points[n].x = cur_x;
	  points[n].y = CORR(cur_y);                      
	  DBUG_PRINT("line",("PointX=%d PointY=%d\n",points[n].x,points[n].y));
        }
	DBUG_PRINT("line",("n=%d\n",n));
	X = cur_x;
	Y = cur_y;
	if (area_Flag){
	  PrepareFillArea(points, fy, Y, n);
	  if (area_Boundary){
	     if (pattern_Flag){
	         SetPatternOFF();
		 DrawLines(points, n);
		 SetPatternON();
	     }
	     else{
	       DrawLines(points, n);
	     }
	  }
	}
	else{
	  if (pattern_Flag){
	      SetPatternOFF();
	      DrawLines(points, n);
              SetPatternON();
	  }
	  else{
	    DrawLines(points, n);
	    }
	}  
	DBUG_RETURN(act);
}


int
em_set_background_mix(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_background_mix");

	DBUG_RETURN(act);
}


int
em_set_character_angle(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_character_angle");

	DBUG_RETURN(act);
}


int
em_set_character_cell(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_character_cell");

	DBUG_RETURN(act);
}


int
em_set_character_direction(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_character_direction");

	DBUG_RETURN(act);
}


int
em_set_character_precision(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_character_precision");

	DBUG_RETURN(act);
}


int
em_set_character_set(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_character_set");

	DBUG_RETURN(act);
}


int
em_set_character_shear(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_character_shear");

	DBUG_RETURN(act);
}


int
em_set_color(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_color");
        DBUG_PRINT("line", ("color : %i  hex:%x",buf[1],buf[1]));
        SetGraphColor(buf[1]);
      	DBUG_RETURN(act);
}


int
em_set_extended_color(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_extended_color");
        DBUG_PRINT("line", ("color : %i  hex:%x",buf[3],buf[3]));
        SetGraphColor(buf[3] & 0x07);  /* buf[2] not used */
	DBUG_RETURN(act);
}


int
em_set_line_type(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_line_type");
        DBUG_PRINT("line",("Line Type: %i\n",buf[1]));
	SetLineType(buf[1]);
	DBUG_RETURN(act);
}


int
em_set_line_width(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_line_width");
        DBUG_PRINT("line",("Line Width: %i\n",buf[1]));
	SetLineWidth(buf[1]);
	DBUG_RETURN(act);
}


int
em_set_marker_cell(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_marker_cell");

	DBUG_RETURN(act);
}


int
em_set_marker_precision(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_marker_precision");

	DBUG_RETURN(act);
}


int
em_set_marker_set(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_marker_set");

	DBUG_RETURN(act);
}


int
em_set_marker_symbol(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_marker_symbol");

	DBUG_RETURN(act);
}


int
em_set_mix(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_mix");
        SetForegroundMix(buf[1]);
	DBUG_RETURN(act);
}


int
em_set_pattern_set(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_pattern_set");

	DBUG_RETURN(act);
}


int
em_set_pattern_symbol(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_pattern_symbol");
        DBUG_PRINT("line",("Pattern Symbol: %i\n",buf[1]));
        SetPatternSym(buf[1]);
	if (!buf[1] || ((buf[1] >= 0x10) && (buf[1] < 0x41))){   /* 0 - default pattern, 16 - solid  */
	   pattern_Flag = FALSE;            /* FIXME : Manual 0 - 16 I get 17   */
	}
	else{
	   pattern_Flag = TRUE;
	}
	DBUG_RETURN(act);
}


int
em_draw_comment(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_draw_comment");

	DBUG_RETURN(act);
}


int
em_draw_noop(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_draw_noop");

	DBUG_RETURN(act);
}


int
em_set_arc_params(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_arc_params");

	DBUG_RETURN(act);
}


int
em_set_current_position(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_set_current_position");
	DBUG_PRINT("line", ("length=%d x=%d y=%d %x %x %x %x \n", buf[1],
				  FIELDLEN(buf + 2), FIELDLEN(buf + 4),
			    (int)buf[2], (int)buf[3], (int)buf[4], (int)buf[5]));
	

	X = SETX(FIELDLEN(buf + 2));
	Y = SETY(FIELDLEN(buf + 4));
	DBUG_RETURN(act);
}


int
em_segment_characteristics(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_segment_characteristics");

	DBUG_RETURN(act);
}


int
em_end_prologue(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_end_prologue");

	DBUG_RETURN(act);
}


int
em_end_symbol_definition(buf,len)
	Byte *buf;
	int len;
{
	int act;

	DBUG_ENTER("em_end_symbol_definition");

	DBUG_RETURN(act);
}


int
PrepareFillArea(ps, fy, ly, n)
     XPoint *ps;
     int fy, ly, n;
{

     static int first_x, first_y;
     static int mult;

     XPoint *new_area;

     if (area_count == 0){
        first_x = ps[0].x;
	first_y = fy;
        mult = 1;
      }
     if ((area_count + n) > (AREASIZE * mult)){
        mult ++;
        new_area = realloc(area_points, mult * AREASIZE * sizeof(XPoint));
	area_points = new_area;
     }
     bcopy(ps, area_points + area_count, n * sizeof(XPoint));
     area_count += n;

     if ((ps[n-1].x == first_x) && (ly == first_y)){
        FillArea(area_points, area_count);
        free(area_points);
	area_count = 0;
      }
}



int
reset_default_values()
{
  DBUG_PRINT("reset",(" default values set \n "));
  SetPatternSym(0);
  pattern_Flag = FALSE;  
  SetGraphColor(0);
}

























