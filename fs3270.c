/*
 *  fs3270.c - simple IBM 3278 emulator designed to be used with 4.2bsd telnet
 *
 *	version 0.0 - October 14, 1984, Paul G. Milazzo, Rice University
 *	version 0.1 - July 9, 1985, Paul G. Milazzo, Rice University
 *		merged contributed FS mode enhancements
 *	version 0.2 - August 5, 1985, Paul G. Milazzo, Rice University
 *		dynamic key bindings, beginnings of better FS algorithms
 *	version 0.3 - September 17, 1985, Paul G. Milazzo, Rice University
 *		faster FS algorithms, working EAU, part of PT order,
 *		correct SYS REQ, now does default & alternate sizes,
 *		works with mismatched real and emulated terminal widths,
 *		has ERASE INPUT, DUP, FIELD MARK keys, correct autoskip,
 *		keyboard locking works again, reset key works with
 *		keyboard locked
 *	version 0.4 - September 18, 1985, Paul G. Milazzo, Rice University
 *		I fixed bugs introduced by the "faster FS algorithms", fixed
 *		an apparently long-standing (but never noticed) bug with
 *		cursor positioning after Erase/Write command, and added a
 *		work-around for an apparent curses bug - insch() ignores
 *		standout mode.  I'm told SYS REQ still doesn't work - sigh...
 *
 *	Copyright (c) October 1984, Paul G. Milazzo, all rights reserved
 */

# include       <stdio.h>
# include       <stdlib.h>	/* malloc/realloc/free prototypes */
# include       <ctype.h>
# include	"telnet.h"
# include	"global.h"
# include	"ibm3270.h"
# include	"3270_enc.h"
# include       "dbug.h"
# include       <assert.h>

#include <X11/Intrinsic.h>	
/* #include <X11/StringDefs.h>*/
#include "ColorScreen.h"

extern ColorScreenWidget screen_widget;

/*  PV   for programmed symbols  */
extern int pat_width;
extern int pat_height;
extern int pat_col;


#define TRUE 1
#define FALSE 0

#define GET_CURR_ATTRIB(p)    (atrbuf[p])
#define DrawString(r,c,ch,l,at,col)  XawColScrDrawString(screen_widget, r, c, ch, l,at,col)
#define Refresh(x,y,xx,yy)    XawColScrRefresh(screen_widget, x, y, xx, yy)
#define ClearScreen()         XawColScrClear(screen_widget)
#define MoveCursor(x,y,color) XawColScrCursorMove(screen_widget, x, y, color)
#define CursorOff()           XawColScrCursorOff(screen_widget)
#define CursorOn()            XawColScrCursorOn(screen_widget)
#define CursorAlt(alt)        XawColScrAltCursor(screen_widget,alt)
#define CursorBlink(b)        XawColScrBlinkCursor(screen_widget,b)
#define status_disp(x,ch)     XawCsModeSetChar(screen_widget,x,ch)

extern Byte	etoa[];	  /* EBCDIC to ASCII translation */
extern Byte	atoe[];	  /* ASCII to EBCDIC translation */
extern Byte	addrmap[];/* 3270 buffer address code map */
/*extern Byte     ebc2cg[]; EBCDIC to ibm-font */
extern Byte     cg2ebc[]; /* Font to EBCDIC */


int graphic_cursor ;
/* global variables from telnet.c */
int		ebcdic = 0;	  /* ==> using 3270 fullscreen mode */
extern Byte		*tbp;	  /* -> telnet's terminal input queue */
extern int		tcc;	  /* size of telnet's terminal input queue */
/*extern Byte		*tfrontp; */ /* -> telnet's terminal output queue */
extern Byte		*nfrontp; /* -> telnet's network output queue */
extern char		*ibm_term; /* Type of ibm terminal to negotiate */
extern char		*real_term; /* The type of terminal I really am */

extern Byte netobuf[];		/* just to check for overflow FIXME */



Byte def_char = 64; /* !!!!!!!!!!  Cw_default_char !!!!!!!!!!!!!!!! */
     
int fixbracket        = FALSE;
static int ptfill     = FALSE;
static int formatted  = FALSE;


static int    screen_size;         /* 1920 for 80x24, 2560 for 32x80 
			       * just the size of the buffer
			       */

static int    screen_lines, 
              screen_columns; /* actual size of visible terminal window */

static int    maxlinelength;  /* max linelength for this term type */
static int    maxlines;       /* max # of lines for this term type */

static int    deflinelength;
static int    deflines;       /* default for this term type */

/* static int    buf_size;        size of current valid logical screen size */
       int    linelength;     /* linelength valid during this write */
static int    lines;          /* # of lines valid during this write */

static Byte  chrbuf[SCREEN_SIZE];
static Attr  atrbuf[SCREEN_SIZE];

#define INC_BA(x)       { if(++x >= screen_size) x = 0; }
#define DEC_BA(x)       { if(--x < 0) x = screen_size - 1; }
#define IS_ATTR(x)      (atrbuf[x] & SATTR_ATTRIB)
#define IS_PROTECTED(x) (atrbuf[x] & SATTR_PROT)
#define IS_MODIFIED(x)  (atrbuf[x] & SATTR_MDT)
#define IS_NUMERIC(x)   (atrbuf[x] & SATTR_NUMERIC)
static int    buffer_addr;		  /* current screen address */

static int    cursor_addr = 0; /* index into chrbuf/attrbuf  */
static int    s_adr;

#define ATTR_NONE   -1
static int    first_attr;
static int    colorfield;
static Attr   defattr, current_attr, current_attr_ra ;
static int    has_graphics = 0 ;

static Byte   last_wcc;
static Bool   last_cmd;
static Bool   base_color = FALSE;

/*
 * Mouse selection (copy).  A rectangle of cells from the anchor (where
 * the button went down) to the extent (where the pointer is now), shown
 * by drawing the cells with reverse video toggled.  sel_on is FALSE
 * until the pointer leaves the anchor cell, so a plain click selects
 * nothing.  Rows and columns are screen cells (0-based).
 */
static int    sel_on = FALSE;
static int    sel_ar, sel_ac;		/* anchor row/column */
static int    sel_er, sel_ec;		/* extent row/column */

struct termtype {
    int		rows;
    int		cols;
    int		defrows;
    int		defcols;
    char	*name;
};

Byte			command_buffer[COMMAND_BUFFER_SIZE];
Byte			*cbufp;
int			keyboard_locked = TRUE;

static char		last_aid = AID_NONE;
static int	        insert = FALSE;



/* imported functions */
void				prof_init ();
char				*getenv ();

/* exported functions */
# include	"keyfunctions.h"

void				fs_decode_command ();
void				fs_init ();
int				fs_select_term ();
static char *			decode_attr();	/* defined static below */

/* private functions */
#define	ADDR_TO_RC(a,r,c) {(r) = (a) / linelength; (c) = (a) % linelength;}
#define	ADDR_TO_R(a)  (a) / linelength
#define	ADDR_TO_C(a)  (a) % linelength

static void			fs_int_start_field ();
static void			add_attribute ();
static void			clear_screen_buffer ();
static void			erase_all_unprotected ();
static void			erase_unprotected_to ();
static void			format_screen ();
static void			fswrite ();
static int			interpret_wcc ();
static void			repeat_to ();
static int			screenaddr ();
static void			send_all_fields ();
static void			send_modified_fields ();
static void			host_read_modified ();
static void			reply_modified_fields ();
static void			send_pa ();
static int			set_attribute ();
static void			set_bufaddr ();
static void                     fillblank();

static char *strcmd();

/* "fake" ASCII character codes for special 3270 functions */

#define	DO_DUP			-1
#define	DO_FIELDMARK		-2

/* initialize for full-screen mode */

void fs_init ()
{
    cbufp = command_buffer;
    clear_screen_buffer ();
    cursor_move(0);
    cursor_on ();
    status_disp (0, CG_BOX4);
    status_disp (1, CG_UNDERA);
    status_disp (2, CG_BOXSOLID);
}

void fs_toggle_basecolor()
{
  base_color = base_color ? FALSE : TRUE;
}
/* clear the screen to all nulls */

static void clear_screen_buffer ()
{
    buffer_addr = 0;
    cursor_addr = 0;
    colorfield = 0;

    bzero((char *)chrbuf, sizeof (chrbuf));  
    bzero((char *)atrbuf, sizeof (atrbuf));  
    sel_on = FALSE;		/* new screen: the old selection is gone */
}

/* fs_decode - decode host data stream.
 *
 *   	This may either be a list of structured fields started by CMD_WSF 
 *	or CMD_MAGIC, or a list of plain 3270 commands. We split the 
 *	deoder because a structured field may itself contain a 
 *	Outbound3270DS sf which is in essence a SNA command sequence.
 *   	If we hit such a Outbbound3270DS in fs_decode_structured_field(),
 *	 we call  fs_decode_command() from there.
 */
void fs_decode(bp,length)
     register Byte *bp;
     int length;
{

    DBUG_PRINT("cmd",("fs_decode length=%d\n",length));
    /* DBUG_HEX("cmd",bp,length); */

   current_attr_ra = -1;  /*  pv initialisation pour RA */
    switch (*bp) {
      case CMD_WSF:
      case CMD_MAGIC:           /* 0x11 - where is this documented? FIXME
				 * Guess:
				 * this could be the 'simplified local host
				 * adapter' WSF command, see 3-79 in 3179
				 * Manual
			         */

	fs_decode_structured_fields(bp,length);
	break;

      default:
	fs_decode_command(bp,length);
    }
    cbufp = command_buffer;
}


/* process a complete 3270 data stream from the host */

void fs_decode_command (bp, length)
     register Byte *bp;
     int length;
{
    register Byte	cmd;
    int erase_flag = FALSE;

    DBUG_ENTER("fs_decode_command");
    
    if (length == 0)
      DBUG_VOID_RETURN;

    cmd = *bp++;    

    DBUG_PRINT("cmd",("fscommand=\"%s\" length=%d\n",strcmd(cmd),length));

    /* first, figure out which command we've been given */
    switch (cmd) {
	
      case CMD_EWA:
      case CMD_SNA_EWA:
      case CMD_EW:
      case CMD_SNA_EW:
	
	if ( (cmd == CMD_EWA) || (cmd == CMD_SNA_EWA)){
	    lines = maxlines;
	    linelength = maxlinelength;
	}
	else {
	    lines = deflines;
	    linelength = deflinelength;
	}

	clear_screen_buffer ();
	if (!has_graphics) clear_window ();

	formatted = FALSE;
	erase_flag = TRUE;

	/* fall through... */
	
      case CMD_WRITE:
      case CMD_SNA_WRITE:
	/* buf_size = lines * linelength; */
	if (erase_flag || !formatted)
	  {
	    current_attr = defattr;
	  }
	else
	  {
	    /* write commands reset attr from last */
	    current_attr = atrbuf[get_field_attribute(buffer_addr)]
	      & ~SATTR_ATTRIB;
	  }
	ptfill = FALSE;
	fswrite (bp,length-2);	/* ORDER_SA */
	last_aid = AID_NONE;
	break;
	
      case CMD_RB:
      case CMD_SNA_RB:
	send_all_fields (last_aid);
	break;
	
      case CMD_RM:
      case CMD_SNA_RM:
	host_read_modified (last_aid);	/* host read: keyboard untouched */
	break;
	
      case CMD_SNA_RMA:
	send_modified_all_fields (last_aid);
	break;
	
       case CMD_EAU:
       case CMD_SNA_EAU:
	erase_all_unprotected ();
	last_aid = AID_NONE;
	fs_reset();
	break;
	
      case CMD_SEL:
      case CMD_SENSE:
      case CMD_SID:
	DBUG_PRINT("bug",("unimplemented command 0x%x\n",*(bp-1)));
	break;
	
      case CMD_NOOP:
	break;
	
      default:
	DBUG_PRINT("bug",("illegal command 0x%x\n",*(bp-1)));
	break;
    }
    
    DBUG_VOID_RETURN;
}


/* Interpret a list of `structured fields'.
 *   bp points to CMD_WSF.
 *   curr_len includes CMD_WSF Byte
 *
 *   Structure of a single 'Structured Field':
 *       | leng-hi | leng-low | CMD | .. | .. | * |
 *       | <------------------ leng --------------^
 */
fs_decode_structured_fields(bp,curr_len)
     register Byte *bp;
     int curr_len;
{
    int curr_off = 1;
    int field_len;
    Byte field_id;
    int read_flag = 0;
    
    DBUG_ENTER("fs_decode_structured_fields");
    
    DBUG_PRINT("wsf",("curr_len=%d\n",curr_len));

    curr_len--;

    /* bp[curr_off points to first (high) length Byte */

    while (curr_len >= 3) {

        field_len = FIELDLEN(bp+curr_off);
	/* field_len = bp[curr_off + 1] + (bp[curr_off] << 8);*/

	if (field_len == 0)	/* indicates last command in `RU chain' */
	  field_len = curr_len;
	field_id = bp[curr_off + 2];

	fs_single_structured_field(bp + curr_off,field_len, &read_flag);
	if (read_flag)
	  break;
	curr_off += field_len;
	curr_len -= field_len;
    }
    DBUG_VOID_RETURN;
}

/* data points to first length Byte of structured field */

fs_single_structured_field(data,len,rdflag)
     Byte *data;
     int len;
     int *rdflag;
{
  register Byte *onfrontp;
     int i,j,k,l,pde,bite,pds,bits;
     char pat_data[10000];  

  DBUG_ENTER("fs_single_structured_field");
  
  
  DBUG_PRINT("isf",("id =%d len=%d\n",data[2],len));
  
  switch (*(data + 2)) {	/* type of SF */
  case OSF_READ_PARTITION:
    if (((data[4] == OSF_RP_TYPE_QUERY) ||
	 (data[4] == OSF_RP_TYPE_QUERY_LIST)) &&
	(data[3] == 0xff))    { /* query operations */
      onfrontp = nfrontp;
      fs_read_partition_query(len, data);
      DBUG_PRINT("rp-reply",("isf total len=%d\n",nfrontp-onfrontp));
    } else {
      DBUG_PRINT("bug",("unimplemented cmd: OSF_READ_PARTITION, sf="));
      DBUG_HEX("isf",data,len);
    }
    *rdflag = 1;
    break;
    
  case OSF_ERASE_RESET:
    DBUG_PRINT("isf",("SF erase-reset\n"));
    clear_screen_buffer();
    clear_window();
    reset_default_values();   /* FIXME not sure !! */
    break;
    
  case OSF_LOAD_PROG_SYMBOL:
    DBUG_PRINT("isf",("SF load-prog-symbol\n"));
      if ((*data == 0) && (*(data+1) == 7))  break;
      if (*(data+3) != 0x83) {
    fprintf(stderr,"\n only PS type 83 supported. type %x unsupported \n",*(data+3));
       break; }
      pat_col = *(data+12) ;
      pat_width=*(data+9);
      pat_height=*(data+10);
     pde=2;
     bite=0;
     bits=0;
     pat_data[0]=0; 
      for ( j= *(data+5) ; j <= *(data+14) ; j++)   /*  character */
{        pds = 0;
         pat_data[pds]=0; 
      for ( k=0 ; k<pat_height ; k++) /* line */ 
     {
      for ( l=0 ; l<pat_width ; l++) /* column ( par bit) */
        { pat_data[pds] += (*(data+15+pde)>>(7-bite) & 0x01) << bits ;
         bite++; if (bite==8) { bite=0; pde++ ; } 
         bits++; if (bits==8) { bits=0; pds++ ; pat_data[pds]=0; } 
        }
      if (bits!=0) { bits=0; pds++ ; pat_data[pds]=0; }
     }
      if (bite!=0) { bite=0; pde++ ; }
      XawCreatePixmap(screen_widget,pat_data,j);
}

    break;
    
  case OSF_SET_REPLY_MODE:
    DBUG_PRINT("isf",("SF set-reply-mode\n"));
    break;
    
  case OSF_CREATE_PARTITION:
    DBUG_PRINT("isf",("SF create-partition\n"));
    break;
    
  case 0x0f:
    DBUG_PRINT("isf",("SF 0x0f? graphics-leadin\n"));
    fs_decode_graphics(data,len);
    break;
    
  case 0x20:		/* file transfer???? */
    DBUG_PRINT("isf",("SF file transfer\n"));
    break;
    
  case OSF_OUTBOUND_3270DS:
    DBUG_PRINT("isf",("SF Outbound3270DS\n"));
    fs_decode_command(data+4,len-4);
    break;
    
  default:
   fprintf(stderr,"\n instruction ignoree %x \n",*(data + 2));
    ;
  }
  DBUG_VOID_RETURN;
}

/* RMA command */

send_modified_all_fields (aid)
     int aid;
{
  DBUG_ENTER("send_modified_all_fields"); /* UNIMPL - FIXME */
  DBUG_VOID_RETURN;
}

#define PSIZE 32768
#define CTRSIZE 32768

#define PMAXSIZE 204800
#define CMAXSIZE 204800

static int pmult=1, cmult=1;

static Byte *picbuff, *newpic;
static Byte *ctrlbuff, *newctr;
static int ccount,pcount = 0;

/*  Structured Field with data[2] = 0x0f
 *
 */
fs_decode_graphics(data,len)
     Byte *data;
     int len;
{
  
  DBUG_ENTER("fs_decode_graphics");
  
  has_graphics = 1;
  switch (data[3]) {
  case OISF_DATA_CHAIN:
    DBUG_PRINT("imp",("Data Chain, RES=0x%x\n",data[4]));
    break;
    
  case OISF_DEST_ORIGIN:
    DBUG_PRINT("imp",("Dest-Origin: FLAGS=0x%x\n",data[4]));
    break;
    
  case OISF_OEM_DATA:
    DBUG_PRINT("imp",("OEM Data: GROUP=0x%x len=%d\n",data[5],len));
    break;
    
  case OISF_SAVE_RESTORE_FMT:
    DBUG_PRINT("imp",("Save/Restore format: FLAGS=0x%x len=%d\n",data[4],len));
    break;
    
  case OISF_SELECT_IPDS:
    DBUG_PRINT("imp",("Select Intelligent Printer DS\n"));
    break;
    
  case OISF_OBJ_DATA:
    DBUG_PRINT("imp",("Object Data: Flags=0x%x objtyp=0x%x len=%d\n",
		      data[5],data[6],len));
    break;
    
  case OISF_OBJ_CONTROL:
    DBUG_PRINT("imp",("Object Control: Flags=0x%x objtyp=0x%x len=%d ccount=%d\n",
		      data[5],data[6],len,ccount));

    assert((ccount + len) < CMAXSIZE);
    
    switch (data[5] & SPAN_MASK) { /* save data if spanned across several sf's */

    case SPAN_FIRST:		/* first but not last */
      ccount = 0;	
      cmult=1;
      ctrlbuff = malloc(cmult*CTRSIZE*sizeof(Byte));

      /* fall through */

    case SPAN_MIDDLE:   	/* not first, not last  */
      if (!((ccount + len) < (cmult*CTRSIZE))){
	cmult++;
	newctr = realloc(ctrlbuff, cmult*CTRSIZE*sizeof(Byte));
	ctrlbuff = newctr;
      }
      bcopy(&data[7], ctrlbuff + ccount, len - 7);
      ccount += (len - 7);
      break;

    case SPAN_LAST:		/* last but not first  */
      if (!((ccount + len) < (cmult*CTRSIZE))){
	cmult++;
	newctr = realloc(ctrlbuff, cmult*CTRSIZE*sizeof(Byte));
	ctrlbuff = newctr;
      }
      bcopy(&data[7], ctrlbuff + ccount, len - 7);
      fs_int_objctrl(ctrlbuff, ccount + len - 7); 
      ccount = 0;
      free(ctrlbuff);
      cmult = 0;
      break;

    case SPAN_ONLY:		/* first and last */
      fs_int_objctrl(&data[7], len - 7); /* no need to save, go execute  */
      ccount = 0;
      break;
    }
    break;
    
  case OISF_OBJ_PICTURE:
    DBUG_PRINT("imp",("Object Picture: Flags=0x%x objtyp=0x%x len=%d pcount=%d\n",
		      data[5],data[6],len,pcount));
   
/*  assert((pcount + len) < PMAXSIZE);   */
    
    switch (data[5] & SPAN_MASK) { /* save data if spanned across several sf's */

    case SPAN_FIRST:		/* first but not last */
      pcount = 0;
      pmult = 1;
      picbuff = malloc(pmult*PSIZE*sizeof(Byte));
      /* fall through */

    case SPAN_MIDDLE:		/* not first, not last  */
      if (!((pcount + len) < (pmult*PSIZE))){
	pmult++;
	newpic = realloc(picbuff, pmult*PSIZE*sizeof(Byte));
	picbuff = newpic;
      }
      bcopy(&data[7], picbuff + pcount, len - 7);
      pcount += (len - 7);
      break;

    case SPAN_LAST:		/* last but not first  */
      if (!((pcount + len) < (pmult*PSIZE))){
	pmult++;
	newpic = realloc(picbuff, pmult*PSIZE*sizeof(Byte));
	picbuff = newpic;
      }
      bcopy(&data[7], picbuff + pcount, len - 7);
      fs_int_objpict(picbuff, pcount + len - 7); 
      pcount = 0;
      free(picbuff);
      pmult = 1;
      break;

    case SPAN_ONLY:		/* first and last */
      fs_int_objpict(&data[7], len - 7); /* no need to save, go execute  */
      pcount = 0;
      break;
    }
     break;
    
  default:
    DBUG_PRINT("bug",("unsupported O/I SF, sfid=0x%x\n",
		      data[2] <<8 + data[3]));
    fprintf(stderr,"unsupported O/I SF, sfid=0x%x\n",
		      data[2] <<8 + data[3]);
    break;
  }
  DBUG_VOID_RETURN;
}

/* Interpret Data following the `Graphic Control' Outbound SF
 * can be one or more procedures starting with a 
 * `Begin Procedure Command'
 */
fs_int_objctrl(buf,len)
     Byte *buf;
     int len;
{
  register Byte *bp = buf; 
  Byte *ctrl_end = buf + len;
  register Byte *proc_end;
  register int l;
  
  DBUG_ENTER("fs_int_objctrl");
  
  while (bp < ctrl_end) {
    
    assert(*bp == 0x30);	/* begin procedure command */

    proc_end = bp + 12 + FIELDLEN(bp + 8);	/* end of this procedure */    
    bp += 12;			/* skip over 'Begin Procedure  */

    /*
     * for each procedure contained in graphic control:
     */
    while (bp < proc_end) {

      switch (*bp) {

      case GERASE:
	em_erase_graphics_presentation_space();
	reset_default_values();
	bp += 2;        
	break;
	
      case GSTOPDR:
	em_stop_draw();     
	bp += 2;      
	break;
	
      case GATTCUR:
	em_attach_graphic_cursor();
	bp += 2; 
	break;
	
      case GDETCUR:
	em_detach_graphic_cursor();
	bp += 2; 
	break;
	
      case GSETCUR:
	l = bp[1] + 2;
	em_set_graphic_cursor_position(bp,l);
	bp += l;
	break;
	
      case GSCUDEF:
	l = bp[1] + 2;
	em_set_current_defaults(bp,l);
	bp +=  l;
	break;
	
      case GNOP1:
	em_proc_noop();
	bp++;
	break;
	
      case GCOMNT:
	l  = 2 + bp[1]; 
	em_proc_comment(bp,l);
	bp += l;
	break;
	
      default:
	DBUG_PRINT("bug",("unknown control opcode 0x%x\n",*bp));
	bp++;
      }
    }
  }
  DBUG_VOID_RETURN;
}

/* Interpret Data following the `Graphic Picture' Outbound SF
 * Followed by one or more `Begin Segment Commands'.
 */
fs_int_objpict(buf,len)
     Byte *buf;
     int len;
{
  register Byte *bp = buf; 
  Byte *pict_end = buf + len;
  register Byte *seg_end;
  register int l;
  
  DBUG_ENTER("fs_int_objpict");
  
  while (bp < pict_end) {
    
   /*  assert(*bp == 0x70);	/* begin segment command */
    if (*bp != 0x70) {
      DBUG_PRINT("bug",("BUG! : not at begin segment, buffer is (len %d):\n",len));
      DBUG_HEX("bug",bp,len);
    } else {
      DBUG_PRINT("obj",("begin segment, length=%d\n",FIELDLEN(bp+8)));
    }
    seg_end = bp + 14 + FIELDLEN(bp + 8);	/* end of this segment */    
    bp += 14;			/* skip over 'Begin Segment  */
    
    
    while (bp < seg_end) {

      l = 2 + bp[1];		/* for most fields, at least */      

      switch (*bp) {
	
      case GBAR:
	em_begin_area(bp);	/* exception : no length field, */
	l = 2;
	break;
	
      case GBIMG:
	em_begin_image(bp,l);
	break;
	
      case GCBIMG:
	em_begin_image_cp(bp,l);
	break;
      case GCHST:
	em_character_string(bp,l);
	break;
	
      case GCCHST:
	em_character_string_cp(bp,l);
	break;
	
      case GEAR:
	em_end_area();
	break;
	
      case GEIMG:
	em_end_image();
	break;
	
      case GFLT:
	em_fillet(bp,l);
	break;
	
      case GCFLT:
	em_fillet_cp(bp,l);
	break;
	
      case GFARC:
	em_full_arc(bp,l);
	break;
	
      case GCFARC:
	em_full_arc_cp(bp,l);
	break;
	
      case GIMD:
	em_image_data(bp,l);
	break;
	
      case GLINE:
	em_line(bp,l);
	break;
	
      case GOLIN:
	em_line_cp(bp,l);
	break;
	
      case GMRK:
	em_marker(bp,l);
	break;
	
      case GCMRK:
	em_marker_cp(bp,l);
	break;
	
      case GRLINE:
	em_relative_line(bp,l);
	break;
	
      case GCRLIN:
	em_relative_line_cp(bp,l);
	break;
	
      case GSBMX:
	em_set_background_mix(bp);
	l = 2;
	break;
	
      case GSCA:
	em_set_character_angle(bp,l);
	break;
	
      case GSCC:
	em_set_character_cell(bp,l);
	break;
	
      case GSCD:
	em_set_character_direction(bp);
	l = 2;
	break;
	
      case GSCR:
	em_set_character_precision(bp);
	l = 2;
	break;
	
      case GSCS:
	em_set_character_set(bp,l);
	l = 2;
	break;
	
      case GSCH:
	em_set_character_shear(bp);
	break;
	
      case GSCOL:
	em_set_color(bp);
	l = 2;
	break;
	
      case GSECOL:
	em_set_extended_color(bp,l);
	break;
	
      case GSLT:
	em_set_line_type(bp);
	l = 2;
	break;
	
      case GSLW:
	em_set_line_width(bp);
	l = 2;
	break;
	
      case GSMC:
	em_set_marker_cell(bp,l);
	break;
	
      case GSMP:
	em_set_marker_precision(bp);
	l = 2;
	break;
	
      case GSMS:
	em_set_marker_set(bp);
	l = 2;
	break;
	
      case GSMT:
	em_set_marker_symbol(bp);
	l = 2;
	break;
	
      case GSMX:
	em_set_mix(bp);
	l = 2;
	break;
	
      case GSPS:
	em_set_pattern_set(bp);
	l = 2;
	break;
	
      case GSPT:
	em_set_pattern_symbol(bp);
	l = 2;
	break;
	
      case GCOMT:
	em_draw_comment();
	break;
	
      case GNOP1:
	em_draw_noop();
	l = 1;
	break;
	
      case GSAP:
	em_set_arc_params(bp,l);
	break;
	
      case GSCP:
	em_set_current_position(bp,l);
	break;
	
      case GSGCH:
	em_segment_characteristics(bp,l);
	break;
	
      case GEPROL:
	em_end_prologue();
	l = 2;
	break;
	
      case GESD:
	em_end_symbol_definition();
	l = 1;
	break;

      case 0x70:   /*   do'nt know what it is */
	break;
	
      default:
	DBUG_PRINT("bug",("unknown picture  opcode 0x%x\n",*bp));
	fprintf(stderr,"unknown picture  opcode 0x%x\n",*bp);
	l = 1;
      }
      bp += l;			/* optimize the common case */
    }
  }
  DBUG_VOID_RETURN;
}


int convcol(attr)
     Attr attr;
{
  if (attr & SATTR_PROT) /* protected */
    {
      if ((attr & SATTR_DISPLAY) == SATTR_HP)
	return WHITE;
      else {
	return base_color ? GREEN : BLUE;
      }
    }
  else	/* unprotected */
    {
      if ((attr & SATTR_DISPLAY) == SATTR_IN) /* high-intensity */
	return base_color ? WHITE : RED;
      else 
	if ((attr & SATTR_DISPLAY) == SATTR_HP) 
	  return base_color ? WHITE : RED;
      else 
	return GREEN;
    }
/*
  switch (attr & SATTR_MASK) 
    {
    case SATTR_PROT :
      return BLUE;
    case (SATTR_HP|SATTR_PROT|SATTR_NUMERIC): 
      return WHITE;
    case (SATTR_IN|SATTR_MDT):
      return RED;
    case SATTR_MDT:
      return GREEN;
    default:
      return BLUE;
    }
*/
}

cursor_on()
{
  DBUG_ENTER("cursor_on");
  DBUG_PRINT("cursor",("em->"));
  CursorOn();
  DBUG_VOID_RETURN;
}
cursor_off()
{
  DBUG_ENTER("cursor_off");
  DBUG_PRINT("cursor",("em->"));
  CursorOff();
  DBUG_VOID_RETURN;
}
cursor_move(addr)
     int addr;
{
  int r,c,color;

  DBUG_ENTER("cursor_move");
  cursor_addr = addr;
  ADDR_TO_RC(addr,r,c);
  DBUG_PRINT("cmd",("cursor_move to x=%d y=%d\n",c,r));
  DBUG_PRINT("cursor",("move_cursor, attr at new pos= 0x%x color=%d\n",
	  atrbuf[addr],convcol(atrbuf[addr])));
  /*
   * Cursor is always green.  It used to take the colour of the field
   * under it, which made it change colour as it moved around.
   */
  MoveCursor(c,r,GREEN);
  DBUG_VOID_RETURN;
}

/*
 * Set the formatted screen flag.  A formatted screen is a screen that
 * has at least one field somewhere on it.
 */
set_formatted ()
{
    register int	baddr;

    formatted = FALSE;
    baddr = 0;
    do {
	if (IS_ATTR (baddr)) {
	    formatted = TRUE;
	    break;
	}
	INC_BA (baddr);
    } while (baddr != 0);
}

/* implement a write command */

static void fswrite (bp,length)
     register Byte	*bp;
     register int length;
{
    register int        sbaddr;
    register int	addr1;
    register int	addr2;
    int			beep;
    register Byte       *last;
    register Byte       ch;
    int                 at;
    int                 old_at;
    int                 erase_flag;

    DBUG_ENTER("fs_decode_command");

    cursor_off();

    status_disp (8, CG_LOCK);
    status_disp (9, CG_BLANK);
/*  hacke par pv
    status_disp (10, CG_CS);
    status_disp (11, CG_CY);
    status_disp (12, CG_CS);
    status_disp (13, CG_CT);
    status_disp (14, CG_CE);
    status_disp (15, CG_CM);   */
    status_disp (10, 0xe2);
    status_disp (11, 0xe8);
    status_disp (12, 0xe2);
    status_disp (13, 0xe3);
    status_disp (14, 0xc5);
    status_disp (15, 0xd4);

    erase_flag = ((*(bp - 1) == CMD_EWA) ||
		  (*(bp - 1) == CMD_EWA));
    
    beep = interpret_wcc (erase_flag,*bp++);
    
    DBUG_PRINT("order", ("left %d :",length));

    last = bp + length;

    while (bp < last) {
	switch (*bp) {
	    
	  case ORDER_SF:
	    DBUG_PRINT("order",("start field"));
	    formatted = TRUE;
	    fs_int_start_field ((Attr) *++bp);
	    ptfill = FALSE;
	    break;
	    
	  case ORDER_SBA:
	    DBUG_PRINT("order",("set buffer address"));
	    fs_int_sba ( bp[1], bp[2]);
	    bp += 2;
	    ptfill = FALSE;
	    break;
	    
	  case ORDER_IC:
  /* pv*/  if (!IS_ATTR(buffer_addr)) cursor_move(buffer_addr);
	    ptfill = FALSE;
	    DBUG_PRINT("order",("insert cursor at %d\n",cursor_addr));
	    break;
	    
	  case ORDER_PT:
	    DBUG_PRINT("order",("program tab"));
	    fs_tab ();
	    break;
	    
	  case ORDER_RA:
	    DBUG_PRINT("order",("repeat to address"));
	    addr1 = (int)*++bp;
	    addr2 = (int)*++bp;
	    ch = *++bp;
	    if (ch == ORDER_GE)
	      {
		ch= *++bp;
	repeat_to (addr1 = screenaddr (addr1, addr2), ch, TRUE);
	      }
	    else 
	      {
	repeat_to (addr1 = screenaddr (addr1, addr2), ch, FALSE);
	      }
	    ptfill = FALSE;
	    break;
	    
	  case ORDER_EUA:
	    DBUG_PRINT("order",("erase unprot to adress"));
	    addr1 = (int)*++bp;
	    erase_unprotected_to (addr1 = screenaddr (addr1, *++bp));
	    ptfill = FALSE;
	    break;
	    
	  case ORDER_GE:
	    DBUG_PRINT("order",("graphics escape"));
	    fs_int_data(bp[1],TRUE);
	    INC_BA(buffer_addr);
	    bp++;
	    break;
	    
	  case ORDER_SFE:
	    DBUG_PRINT("order",("start field extended"));
	    fs_start_field_extended(bp[1],bp + 2);
	    bp += ((unsigned)bp[1]) * 2 + 1;
	    ptfill = FALSE;
	    break;
	    
	  case ORDER_MF:
	    DBUG_PRINT("order",("modify field, npairs=%d 0x%x 0x%x", bp[1], bp[2],bp[3]));
	    fs_modify_field_att(bp[1], bp + 2);
	    bp += ((unsigned)bp[1]) * 2 + 1;
	    ptfill = FALSE;
	    break;
	    
	  case ORDER_SA:
	    DBUG_PRINT("order",("set attribute cmd 0x%x type 0x%x value 0x%x left %d", 
				*bp, *(bp+1), *(bp+2), cbufp-bp));
	    fs_int_sa(bp + 1);
	    bp += 2;
	    ptfill = FALSE;
	    break;

	  default:
	    DBUG_PRINT("order-text",("%c 0x%x at 0x%x",etoa[*bp], *bp,current_attr));
	    ch = *bp;
	    chrbuf[buffer_addr] = ch;
	    if (atrbuf[buffer_addr] & SATTR_ATTRIB)
	    { sbaddr = buffer_addr;
	      INC_BA(sbaddr);
	      while (!IS_ATTR(sbaddr)) {
		atrbuf[sbaddr] = (current_attr & ~0xff)
			     + (atrbuf[sbaddr] & 0xff);
	      INC_BA(sbaddr);}
	    };
	    atrbuf[buffer_addr] = current_attr ;
	    if (((ch == 0xad) || (ch == 0xbd)) &&
		fixbracket) 
		atrbuf[buffer_addr] |= SATTR_GRXESC;
	    screen_update(buffer_addr);
	    INC_BA(buffer_addr);
	    ptfill = TRUE;
	    break;
	}
	bp++;
    }
    if (!formatted)
      set_formatted();
    cursor_on();
    
    if (beep)
      fs_alarm ();
    DBUG_VOID_RETURN;
}

/* interpret a Write Control Character */

static int 
  interpret_wcc (erase_flag,wcc)
int     erase_flag;
Byte	wcc;
{
    register int i;

    DBUG_ENTER("interpret_wcc");
    
    if (wcc & WCC_RESTORE_KBD)
      {
	fs_reset();
      }

    if (wcc & WCC_RMDT) {	/* Reset all Modified Data Tags */
        for (i=0; i < linelength * lines; i++) 
	  atrbuf[i] &= (~SATTR_MDT);
    }

    if (erase_flag && (wcc & WCC_RESET))
      clear_window();

    DBUG_RETURN(wcc & WCC_ALARM);
}


/*
 * merge_sa_attr - lay the character attributes of the last Set Attribute
 * order (current_attr_ra) over a field's own attributes.
 *
 * The low byte holds two independent parts: the colour nibble
 * (SATTR_COLORMASK) and the highlight/transparency nibble (0xf0).  An SA
 * value of 0 in a part means "default", i.e. take it from the field.
 * The old code copied SA's whole low byte, so the very common
 * "SA highlight=default, SA colour=default" reset (3270BBS sends one in
 * every chat header) zeroed the colour of every field started after it
 * in the same write, and they all fell back to the protected base
 * colour.  Only a part SA actually set (non-zero) may override the field.
 */
static Attr
  merge_sa_attr (field_attr)
     Attr	field_attr;
{
  if (current_attr_ra == -1 || current_attr_ra == 0)
    return field_attr;			/* no SA in this write */
  if (current_attr_ra & SATTR_COLORMASK)
    field_attr = (field_attr & ~SATTR_COLORMASK)
      | (current_attr_ra & SATTR_COLORMASK);
  if (current_attr_ra & 0xf0)
    field_attr = (field_attr & ~0xf0) | (current_attr_ra & 0xf0);
  return field_attr;
}


/* stuff an regular (i.e. non-extended) attribute
 * Byte into the current screen position 
 */
static void 
  fs_int_start_field (attr)
     Attr	attr;
{
  static char ebcdic_blank = 0x40;
  int r,c;
  Attr atrprec;
  register int sbaddr = buffer_addr;

  DBUG_ENTER("fs_int_start_field");

  chrbuf[buffer_addr] = EBCDIC_NUL; /*ORDER_SF;*/
  current_attr = SATTR_ATTRIB + ((attr << 8) & SATTR_MASK);

  atrbuf[buffer_addr] = current_attr;

  current_attr &= ~SATTR_ATTRIB;
  INC_BA(sbaddr);

  while (!IS_ATTR(sbaddr)) {
      atrbuf[sbaddr] = (current_attr & ~0xff) + (atrbuf[sbaddr] & 0xff) ;
      screen_update(sbaddr);
  /*   INC_BA(sbaddr); */
    if (++sbaddr  >= screen_size) break;  /* un champ ne wrappe pas ??? */
  }

  current_attr = merge_sa_attr(current_attr);

  ADDR_TO_RC(buffer_addr,r,c);

  DrawString(r, c, &ebcdic_blank, 1,0,0);

  DBUG_PRINT("",("start field: new curr_attr = 0x%x at row %d col %d",attr,r,c));
  INC_BA(buffer_addr);

  DBUG_VOID_RETURN;
}


/*
 * number of attribute/value pairs
 * ponter to first attribute type
 */
fs_start_field_extended(npairs, aptr)
     Byte npairs, *aptr;
{
  register int sbaddr = buffer_addr;
  DBUG_ENTER("fs_start_field_extended");
  
  atrbuf[buffer_addr] = current_attr
    = fs_int_attr(SATTR_ATTRIB, aptr, npairs);
  chrbuf[buffer_addr] = EBCDIC_NUL; /* ORDER_SF; */



  current_attr &= ~SATTR_ATTRIB;
  INC_BA(sbaddr);
  current_attr = merge_sa_attr(current_attr);

   while (!IS_ATTR(sbaddr)) {
  atrbuf[sbaddr] = (current_attr & ~0xff) + (atrbuf[sbaddr]  & 0xff);
      atrbuf[sbaddr] = current_attr ;
       screen_update(sbaddr);
    if (++sbaddr  >= screen_size) break;  /* un champ ne wrappe pas ??? */
  }

  DBUG_PRINT("",("start field extended: new curr attr = 0x%x",current_attr));
  screen_update(buffer_addr);
  INC_BA(buffer_addr);
  DBUG_VOID_RETURN;
}



/*
 * Modify Field Attributes                    travail a terminer:
 *    number of attribute/value pairs          ne modifie que ce qui est
 *    pointer to type byte of first a/v pair   demande
 */
fs_modify_field_att(npairs, aptr)
     Byte npairs, *aptr;
{
    Attr cattr;
    register int sbaddr = buffer_addr;
     DBUG_ENTER("fs_modify_field_att");
   if (atrbuf[buffer_addr] & SATTR_ATTRIB)
{
   current_attr = atrbuf[buffer_addr] & ~SATTR_ATTRIB;
   cattr = fs_int_attr(SATTR_ATTRIB, aptr, npairs) & ~SATTR_ATTRIB;
   if (cattr & 0xff) fs_start_field_extended(npairs, aptr);
   else  INC_BA(buffer_addr);
  }
    DBUG_VOID_RETURN;
}


/* calculate the screen address given the two address Bytes */

static int screenaddr (addr1, addr2)
     int	addr1;
     int	addr2;
{
   register int a;

   a = (addr1 & TWELVE_BIT)
     ? ((addr1 & ADDR_PART) << 6) + (addr2 & ADDR_PART)
       : ((addr1 & ADDR_PART) << 8) + addr2;
   a %= screen_size;
   return a;
}


fs_int_sba(adr1,adr2)
     Byte adr1,adr2;
{
  register int sbaddr ;
  buffer_addr = screenaddr(adr1,adr2);
/*  il faut retrouver le current_attr  */
   sbaddr = buffer_addr;
   DEC_BA(sbaddr);
  /* the field at the new address, with any SA in force laid over it */
  current_attr = merge_sa_attr(atrbuf[get_field_attribute(sbaddr)]
			       & ~SATTR_ATTRIB);
}

/* implement the Repeat to Address order */

static void repeat_to (addr, ch, ge_flag)
     register int	addr;
     Byte	ch;
     int ge_flag;
{
    register int	screensize = linelength * lines;
    register int	bytenumber = buffer_addr;
    
    addr %= screensize;
    
    do {
      fs_int_data(ch,ge_flag);
      INC_BA(buffer_addr);
    } while (buffer_addr != addr);
}

/* implement the Erase Unprotected to Address order */

static void erase_unprotected_to (baddr)
     register int	baddr;
{
    baddr %= screen_size;
    do {
      if (IS_ATTR(buffer_addr))
	current_attr = (atrbuf[buffer_addr] & ~0x00ff)
		    + (current_attr & 0x00ff);
      else if (!IS_PROTECTED (buffer_addr)) 
	{
	  chrbuf[buffer_addr] = CG_NULLBLANK;
	  screen_update (buffer_addr);
	}
      INC_BA (buffer_addr);
    } while (buffer_addr != baddr);
    current_attr = (atrbuf[buffer_addr] & ~0x00ff)
		 + (current_attr & 0x00ff);
    last_cmd = TRUE;
}

/* send a PA or CLEAR key */
static void send_pa (aid)
     int	aid;
{

  key_AID();
#   ifndef STICKYINSERT
  insert_mode(FALSE);
#   endif
    
  /* construct a 3270 "short read" data stream */
  *nfrontp++ = aid;
  *nfrontp++ = IAC;
  *nfrontp++ = EOR;
  last_aid = aid;
}

static void erase_all_unprotected ()
{
  register int baddr,sbaddr;
  Bool f;
  
  cursor_off();
  
  if (!formatted) 
    {
      clear_screen_buffer();
      clear_window();
      cursor_move(0);
    }
  else
    {				/* find first attribute */
      baddr = 0;
      do {
	if (IS_ATTR(baddr))
	  break;
	INC_BA(baddr);
      } while (baddr != 0);
      sbaddr = baddr;
      f = FALSE;
      /* baddr now points to first attribute character */
      do {
	if (!IS_PROTECTED(baddr))      
	  {
	    atrbuf[baddr] &= ~SATTR_MDT;  /* reset MDT */
	    do {
	      INC_BA(baddr);	          /*  baddr = *first unprot. char*/
	      if (!f)
		{
		  cursor_move(baddr);
		  f = TRUE;
		}
	      if (!IS_ATTR(baddr))          /* not yet at next field */
		{
		  atrbuf[baddr] &= ~SATTR_MDT;
		  chrbuf[baddr] = EBCDIC_NUL;
		  screen_update(baddr);
		}
	    } while (!IS_ATTR(baddr));
	  }
	else 
	  {			/* a protected field, skip it */
	    do {
	      INC_BA(baddr);
	    }  while  (!IS_ATTR(baddr));
	  }
      } while (baddr != sbaddr);
      if (!f)
	cursor_move(0);
    }
  cursor_on();
}

/* refresh the character at addr, chrbuf,atrbuf are valid 
 * do *lots* of optimisation here!
 */

/* is buffer address addr inside the selection rectangle? */
static int sel_contains (addr)
     int	addr;
{
  int r, c;

  if (!sel_on)
    return FALSE;
  ADDR_TO_RC(addr, r, c);
  return r >= min(sel_ar, sel_er) && r <= max(sel_ar, sel_er)
      && c >= min(sel_ac, sel_ec) && c <= max(sel_ac, sel_ec);
}

/* the attribute a cell is drawn with: selected cells have reverse
 * video toggled (so a reverse field shows normal while selected) */
static Attr disp_attr (addr)
     int	addr;
{
  Attr a = atrbuf[addr];

  if (sel_contains(addr))
    a ^= SATTR_D_REVERSE;
  return a;
}

screen_update(addr)
     int addr;
{
  int r,c;
  static char ebcdic_blank = 0x40;
  char text  = chrbuf[addr];
  Attr attr  = disp_attr(addr);
  int curr_color = convcol(attr) & SATTR_COLORMASK;
 
  ADDR_TO_RC(addr,r,c);

/*  si graphiques ne pas dessiner les nuls  */
  if (!deferred_update | has_graphics)  {
  if (((attr & SATTR_IN)) == SATTR_IN  )
    DrawString(r, c, &ebcdic_blank, 1,attr,curr_color);
  else
    if (text != EBCDIC_NUL | !has_graphics)
       DrawString(r, c, &text, 1,attr,curr_color);}
  else {
  if (!(attr & SATTR_ATTRIB))  {
  us_start_row = min (us_start_row,r);
  us_stop_row = max (us_stop_row,r);
  us_start_col = min (us_start_col,c);
  us_stop_col = max (us_stop_col,c+1);}
  else DrawString(r, c, &ebcdic_blank, 1,(attr & ~SATTR_D_REVERSE),
     curr_color);        }
}


/*
 * ---- mouse selection, copy and paste (called from eventhdlr.c) ----
 */

void fs_select_clear ();

/* keep a cell position on the current screen */
static void sel_clamp (r, c)
     int	*r, *c;
{
  *r = max(0, min(*r, lines - 1));
  *c = max(0, min(*c, linelength - 1));
}

/* redraw the cells from (top,left) to (bottom,right) inclusive */
static void sel_redraw (top, left, bottom, right)
     int	top, left, bottom, right;
{
  int r, c;

  cursor_off();
  for (r = top; r <= bottom; r++)
    for (c = left; c <= right; c++)
      screen_update(r * linelength + c);
  cursor_on();
}

/* mouse button pressed on cell (r,c): drop any old selection, anchor
 * a new one there */
void fs_select_start (r, c)
     int	r, c;
{
  fs_select_clear();
  sel_clamp(&r, &c);
  sel_ar = sel_er = r;
  sel_ac = sel_ec = c;
}

/*
 * Pointer dragged to cell (r,c).  The selection becomes visible once the
 * pointer moves off the anchor cell.  Only the bounding box of the old
 * and the new rectangle can change, so only that is redrawn.
 */
void fs_select_extend (r, c)
     int	r, c;
{
  int top, left, bottom, right;

  sel_clamp(&r, &c);
  if (r == sel_er && c == sel_ec)
    return;
  /* box spanning the anchor, the old extent and the new one */
  top    = min(sel_ar, min(sel_er, r));
  bottom = max(sel_ar, max(sel_er, r));
  left   = min(sel_ac, min(sel_ec, c));
  right  = max(sel_ac, max(sel_ec, c));
  sel_er = r;
  sel_ec = c;
  sel_on = TRUE;
  sel_redraw(top, left, bottom, right);
}

/* is a selection shown? */
int fs_select_active ()
{
  return sel_on;
}

/* remove the selection highlight */
void fs_select_clear ()
{
  if (!sel_on)
    return;
  sel_on = FALSE;
  sel_redraw(min(sel_ar, sel_er), min(sel_ac, sel_ec),
	     max(sel_ar, sel_er), max(sel_ac, sel_ec));
}

/*
 * Copy the selected cells into buf as Latin-1 text: one line per row,
 * trailing blanks dropped, rows joined by '\n'.  Field attribute cells
 * and nulls become blanks; cells of non-display fields (passwords) are
 * copied as blanks too, so a selection never reveals hidden input.
 * Returns the text length (buf is always NUL-terminated).
 */
int fs_select_text (buf, size)
     char	*buf;
     int	size;
{
  int r, c, n = 0, line_start, addr;
  unsigned char ch;

  if (size < 1)
    return 0;
  if (sel_on) {
    for (r = min(sel_ar, sel_er); r <= max(sel_ar, sel_er); r++) {
      if (r != min(sel_ar, sel_er) && n < size - 1)
	buf[n++] = '\n';
      line_start = n;
      for (c = min(sel_ac, sel_ec); c <= max(sel_ac, sel_ec); c++) {
	addr = r * linelength + c;
	if (IS_ATTR(addr)
	    || (atrbuf[addr] & SATTR_DISPLAY) == SATTR_IN)
	  ch = ' ';
	else
	  ch = etoa[chrbuf[addr]];
	if (n < size - 1)
	  buf[n++] = ch;
      }
      while (n > line_start && buf[n - 1] == ' ')	/* trim the row */
	n--;
    }
  }
  buf[n] = '\0';
  return n;
}

/* plain click on cell (r,c): move the cursor there */
void fs_cursor_to (r, c)
     int	r, c;
{
  sel_clamp(&r, &c);
  cursor_move(r * linelength + c);
}

/* can ch be typed at the cursor without locking the keyboard?  These
 * are the checks fs_character makes before it locks. */
static int paste_ok (ch)
     int	ch;
{
  if (keyboard_locked)
    return FALSE;
  if (IS_ATTR(cursor_addr) || IS_PROTECTED(cursor_addr))
    return FALSE;
  if (IS_NUMERIC(cursor_addr) && !(isdigit(ch) || ch == '.'))
    return FALSE;
  return TRUE;
}

/*
 * Paste Latin-1 text at the cursor as if it were typed.  A newline acts
 * like the Return key (first input field of the next line), a tab like
 * Tab.  Text that does not fit - it reaches a protected cell - is dropped
 * up to the next newline instead of locking the keyboard, so a long
 * paste fills the field and stops.  A trailing newline is ignored, so
 * pasting a copied line leaves the cursor at its end.
 */
void fs_paste (text, len)
     unsigned char	*text;
     int		len;
{
  int i, ch, skip = FALSE;

  for (i = 0; i < len; i++) {
    if (keyboard_locked)
      break;
    ch = text[i];
    if (ch == '\r') {			/* CR LF and lone CR: newline */
      if (i + 1 < len && text[i + 1] == '\n')
	continue;
      ch = '\n';
    }
    if (ch == '\n') {
      if (i == len - 1)
	break;
      fs_return();
      skip = FALSE;
      continue;
    }
    if (skip)
      continue;
    if (ch == '\t') {
      fs_tab();
      continue;
    }
    if (ch < 0x20 || (ch >= 0x7f && ch < 0xa0))	/* other controls */
      continue;
    if (!paste_ok(ch)) {
      skip = TRUE;
      continue;
    }
    fs_character(ch);
  }
}


static void send_all_fields (aid)
     int	aid;
{
  register int i = 0;
  
  /* construct a 3270 data stream in response to a Read Buffer */
  /* send the entire screen, don't worry about wrap or anything */
  *nfrontp++ = aid;
  *nfrontp++ = addrmap[cursor_addr >> 6];
  *nfrontp++ = addrmap[cursor_addr & ADDR_PART];
  
  while (i < screen_size)
    {
      if (IS_ATTR(i))
	{
	  *nfrontp++ = ORDER_SF;
	  *nfrontp++ = (atrbuf[i] >> 8) | SATTR_TOP;
	}
      else 
	{
	  *nfrontp++ = chrbuf[i];
	}
      i++;
    } 
  *nfrontp++ = IAC;
  *nfrontp++ = EOR;
  last_aid = aid;
}


/* construct a 3270 data stream as if responding to a Read Modified */

/*
 * An AID key (Enter, PFn, SysReq) sends the modified fields AND locks the
 * keyboard until the host answers.  A host-initiated Read Modified
 * command must only send them: on a real 3270 a read command never
 * touches the keyboard.  The old code treated both alike, so every host
 * Read Modified locked x3179's keyboard and characters typed until the
 * host's next unlocking write were thrown away.  3270BBS polls the chat
 * input field this way every 2 seconds for its "is typing" indicator.
 */
static void send_modified_fields (aid)
     int	aid;
{
  reply_modified_fields (aid, TRUE);
}

/* answer a host Read Modified command: no keyboard lock, insert mode kept */
static void host_read_modified (aid)
     int	aid;
{
  reply_modified_fields (aid, FALSE);
}

/*
 * Build the inbound Read Modified data stream.  from_key is TRUE when an
 * AID key caused it; only then are the AID-key side effects applied
 * (keyboard lock, insert mode off, "X clock" status indicator).
 */
static void reply_modified_fields (aid, from_key)
     int	aid;
     int	from_key;
{
  register int baddr,sbaddr;

  DBUG_ENTER("send_modified_fields");

  if (from_key) {
    key_AID();

#   ifndef STICKYINSERT
    insert_mode(FALSE);
#   endif
  }

  if (aid == AID_SYSREQ) {	/* construct TEST REQUEST header */
    *nfrontp++ = EBCDIC_SOH;
    *nfrontp++ = atoe['%'];
    *nfrontp++ = atoe['/'];
    *nfrontp++ = EBCDIC_STX;
  }
  else {			/* construct read header */
    if (from_key)
      keyboard_locked = TRUE;
/*  si curseur graphique affiche envoyer la position */
    if (graphic_cursor) {
        em_detach_graphic_cursor();
      *nfrontp++ = 0x88;   /*  AID */
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x34; 
      *nfrontp++ = 0x0f;
 
      *nfrontp++ = 0x0f; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0xc0; 
      *nfrontp++ = 0x00; 

      *nfrontp++ = 0x40; 
      *nfrontp++ = 0x08; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 

      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 

      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x23; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x23;
 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x1f;
 
      *nfrontp++ = 0x01; 
      *nfrontp++ = xcurs >> 8 & 0x000000ff;  /* position x */
        if ((xcurs >> 8 & 0x000000ff) == IAC) *nfrontp++ = IAC; /* escape si necessaire */
      *nfrontp++ = xcurs  & 0x000000ff  ; 
        if ((xcurs  & 0x000000ff) == IAC) *nfrontp++ = IAC;
      *nfrontp++ = ycurs >> 8 & 0x000000ff;  /* position y */
        if ((ycurs >> 8 & 0x000000ff) == IAC) *nfrontp++ = IAC;
 
      *nfrontp++ = ycurs  & 0x000000ff ; 
        if ((ycurs  & 0x000000ff) == IAC) *nfrontp++ = IAC;
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00;
 
      *nfrontp++ = 0x04; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x04; 
      *nfrontp++ = 0x00;

      *nfrontp++ = 0x01; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00;
 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00;
 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 

      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 

      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x00; 
      *nfrontp++ = 0x80; 
 
      *nfrontp++ = 0x00;  }

    *nfrontp++ = aid;
    *nfrontp++ = addrmap[(cursor_addr >> 6) & ADDR_PART];
    *nfrontp++ = addrmap[cursor_addr & ADDR_PART];
  }
  DBUG_PRINT("send_modified",("formatted=%s\n",formatted ? "TRUE":"FALSE"));

  baddr = 0;
  if (formatted) {
    /* find first field attribute */
    do {
      if (IS_ATTR(baddr))
	break;
      INC_BA (baddr);
    } while (baddr != 0);
    sbaddr = baddr;
    do {
      if (IS_MODIFIED (baddr)) {
	DBUG_PRINT("send_modified",("fmted-MODIFY at %d\n",baddr));
	INC_BA (baddr);
	*nfrontp++ = ORDER_SBA;
	*nfrontp++ = addrmap[(baddr >> 6) & 0x3F];
	*nfrontp++ = addrmap[baddr & 0x3F];
	do {
	  if (chrbuf[baddr] != EBCDIC_NUL) /* FIXME - font dependent NUL test -mah */
	    {
	      *nfrontp++ = chrbuf[baddr];
	      DBUG_PRINT("send_modified",("fmted-adding char=%c\n",etoa[chrbuf[baddr]]));
	    }
	  INC_BA (baddr);
	} while (!IS_ATTR(baddr));
      }
      else {	/* not modified - skip */
	do {
	  INC_BA (baddr);
	} while (!IS_ATTR(baddr));
      }
    } while (baddr != sbaddr);
  }
  else {
    do {
      if (chrbuf[baddr] != EBCDIC_NUL) /* FIXME - font dependent NUL test -mah */
	{
	  *nfrontp++ = chrbuf[baddr];
	  DBUG_PRINT("send_modified",("unfmted-adding char=%c\n",etoa[chrbuf[baddr]]));
	}
      INC_BA (baddr);
    } while (baddr != 0);
  }
  
  *nfrontp++ = IAC;
  *nfrontp++ = EOR;
  last_aid = aid;
  
  DBUG_VOID_RETURN;
}

/* 
 * necessary strangeness for the Query Reply Inbound Structured Field
 * For details, see `Functional Description', GA23-0218-05, pp 3-38
 */
struct attr {
    Byte type;
    Byte flags;
#define DEFAULT 0x01
    unsigned length;
    Byte *value;
};

/* the following 4 usable area definitions depend on the
 * font used and differ only in the 'default cell width, height'
 * and 'AUA width/height' fields.
 * This information should be read from the font in use and
 * inserted into the proper field.
 */
/* original */
/*
static Byte uarea[] = {
    0x81, 0x81, 0x03, 0x00, 0x00, 0x50, 0x00, 0x18,
    0x00, 0x00, 0x01, 0x00, 0x48, 0x00, 0x01, 0x00,
    0x48, 0x06, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
  
    0x00, 0x13, 0x02, 0x00, 0x01, 0x00, 0x50, 0x00,
    0x18, 0x00, 0x00, 0x01, 0x00, 0x48, 0x00, 0x01,
    0x00, 0x48, 0x06, 0x0C
  
  };

*/

/*


static Byte uarea8024[] = {
    0x81, 0x81, 0x03, 0x00, 0x00, 0x50, 0x00, 0x18,
    0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x04, 0x00,
    0x09, 0x09, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x13, 0x02, 0x00, 0x01, 0x00, 0x50, 0x00,
    0x20, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x04,
    0x00, 0x09, 0x09, 0x0C
  
  };
*/

static Byte uarea[] = {
    0x81, 0x81, 0x03, 0x00, 0x00, 0x50, 0x00, 0x20,
    0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00,
    0x03, 0x09, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x13, 0x02, 0x00, 0x01, 0x00, 0x50, 0x00,
    0x20, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01,
    0x00, 0x03, 0x09, 0x10
  
  };

/*
static Byte uarea[] = {   80 x 32  experimental
    0x81, 0x81, 0x03, 0x00, 0x00, 0x50, 0x00, 0x20,
    0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00,
    0x03, 0x09, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x13, 0x02, 0x00, 0x01, 0x00, 0x50, 0x00,
    0x20, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01,
    0x00, 0x03, 0x09, 0x0C
  
  };
*/
/* original 
static Byte implpart[] = {
    0x81, 0xA6, 0x00, 0x00, 0x0B, 0x01, 0x00, 0x00,
    0x50, 0x00, 0x18, 0x00, 0x50, 0x00, 0x18, 0x0B,
    0x02, 0x00, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x06,
    0x00, 0x0C
  };
*/
static Byte implpart[] = {
    0x81, 0xA6, 0x00, 0x00, 0x0B, 0x01, 0x00, 0x00,
    0x50, 0x00, 0x18, 0x00, 0x50, 0x00, 0x20, 0x0B,
    0x02, 0x00, 0x00, 0x09, 0x00, 0x10, 0x00, 0x09,
    0x00, 0x10
  };


static Byte replymode[] = {
    0x81, 0x88, 0x00, 0x01, 0x02
  };

static Byte summary[] = {
    0x81, 0x80, 0x80, 0x81, 0x84, 0x85, 0x86, 0x87,
    0x88, 0xA1, 0xA6, 0xA8, 0x96, 0x99, 0xB0, 0xB1,
    0xB2, 0xB3, 0xB4, 0xB6
  };

static Byte alphapart[] = {
    0x81, 0x84, 0x00, 0x0a, 0x00, 0x04
  };

static Byte storagepool[] = {
    0x81, 0x96, 0x0D, 0x01, 0x01, 0x00, 0x00, 0x6C,
    0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x07
  };

static Byte auxdev[] = {
    0x81, 0x99, 0x00, 0x00
  };

static Byte rpqnames[] = {	/* GFTM version 02.05 ???? */
    0x81, 0xA1, 0xC7, 0xC6, 0xE3, 0xD4, 0x00, 0x00,
    0x00, 0x00, 0x06, 0xF0, 0xF2, 0x4B, 0xF0, 0xF5
  };

static Byte backgrdtransp[] = {
    0x81,	0xA8, 0x02, 0x00, 0xF0, 0xFF, 0xFF
  };

static Byte segment[] = {
    0x81, 0xB0, 0x00, 0x02, 0x00, 0x00, 0x00, 0xFF,
    0xFF, 0x05, 0x02, 0x00, 0x80, 0x00, 0x05, 0x02,
    0x04, 0x80, 0x00, 0x05, 0x02, 0x0B, 0x80, 0x00
  };                /*^^^^^^^^^^^^^^^^^^^^^^^^^^ 
		     * very dubios. Manual sez its'21 Bytes, resources has26! FIXME
		     */


static Byte procedure[] = {
    0x81, 0xB1, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF,
    0xFF, 0x06, 0x00, 0x06, 0x00, 0x06, 0x01, 0xFF,
    0xFF, 0xff, 0xf0
  };

static Byte linetype[] = {
    0x81, 0xB2, 0x00, 0x09, 0x00, 0x07, 0x01, 0x01,
    0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 
    0x06, 0x06, 0x07, 0x07, 0x08, 0x08 
};


static Byte port1[] = {
    0x81, 0xB3, 0x00, 0x05, 0x02, 0x90, 0x09, 0x01,
    0x00, 0x01, 0x02, 0x80, 0x00, 0x7F, 0xFF
  };


static Byte port2[] = {
    0x81, 0xB3, 0x00, 0x07, 0x08, 0x40, 0x07, 0x03,
    0x00, 0x01, 0x00, 0x00, 0x1C
  };

static Byte graphiccolor[] = {/* ok */
    0x81, 0xB4, 0x00, 0x04, 0x00, 0x01, 0xFF, 0x00,
    0x08, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x05, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x06, 0x01, 0x00, 0x01, 0x00, 0x00, 0x07, 0x01,
    0x01, 0x01, 0x00
  };

/* original
static Byte graphicsymset[] = {
    0x81, 0xB6, 0x70, 0x80, 0x06, 0x0C, 0x01, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00,
    0x00, 0x10, 0x01, 0x00, 0x00, 0xF0, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xB9, 0x00,
    0x25, 0x10, 0x01, 0x00, 0x00, 0xF0, 0x02, 0x02,
    0x00, 0x00, 0x06, 0x0C, 0x00, 0x02, 0xB9, 0x00,
    0x25, 0x10, 0x01, 0x00, 0x00, 0xF0, 0x83, 0x83,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xB9, 0x00,
    0x25, 0x0B, 0x01, 0x02, 0x80, 0xFF, 0x02, 0x02,
    0x01, 0x01, 0x06, 0x0C, 0x0B, 0x01, 0x03, 0x80,
    0xFF, 0x02, 0x02, 0x01, 0x01, 0x06, 0x0C, 0x0B,
    0x01, 0x08, 0x88, 0xFF, 0x02, 0x02, 0x01, 0x01,
    0x06, 0x0C, 0x0B, 0x01, 0x09, 0x88, 0xFF, 0x02,
    0x02, 0x01, 0x01, 0x06, 0x0C, 0x0B, 0x01, 0x0A,
    0x88, 0xFF, 0x02, 0x02, 0x01, 0x01, 0x06, 0x0C,
    0x0B, 0x01, 0x0B, 0x88, 0xFF, 0x02, 0x02, 0x01,
    0x01, 0x06, 0x0C
  };
*/
static Byte graphicsymset[] = {
    0x81, 0xB6, 0x70, 0x80, 0x09, 0x0C, 0x01, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00,
    0x00, 0x10, 0x01, 0x00, 0x00, 0xF0, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xB9, 0x00,
    0x25, 0x10, 0x01, 0x00, 0x00, 0xF0, 0x02, 0x02,
    0x00, 0x00, 0x06, 0x0C, 0x00, 0x02, 0xB9, 0x00,
    0x25, 0x10, 0x01, 0x00, 0x00, 0xF0, 0x83, 0x83,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xB9, 0x00,
    0x25, 0x0B, 0x01, 0x02, 0x80, 0xFF, 0x02, 0x02,
    0x01, 0x01, 0x06, 0x0C, 0x0B, 0x01, 0x03, 0x80,
    0xFF, 0x02, 0x02, 0x01, 0x01, 0x06, 0x0C, 0x0B,
    0x01, 0x08, 0x88, 0xFF, 0x02, 0x02, 0x01, 0x01,
    0x06, 0x0C, 0x0B, 0x01, 0x09, 0x88, 0xFF, 0x02,
    0x02, 0x01, 0x01, 0x06, 0x0C, 0x0B, 0x01, 0x0A,
    0x88, 0xFF, 0x02, 0x02, 0x01, 0x01, 0x06, 0x0C,
    0x0B, 0x01, 0x0B, 0x88, 0xFF, 0x02, 0x02, 0x01,
    0x01, 0x06, 0x0C
  };


static void fs_add_sf();

static Byte nocolor[] = {	/* unused FIXME */
    0x81, 0x86, 0x00, 0x08, 0x00, 0xF4, 0xF1, 0x00,
    0xF2, 0x00, 0xF3, 0x00, 0xF4, 0x00, 0xF5, 0x00,
    0xF6, 0x00, 0xf7, 0x00
  };

static Byte color[] = { /* ok */
    0x81, 0x86, 0x00, 0x08, 0x00, 0xF4, 0xF1, 0xF1,
    0xF2, 0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF5, 0xF5,
    0xF6, 0xF6, 0xf7, 0xF7, 0x00, 0x0D
  };

static Byte highlight[] = { /* ok */
    0x81, 0x87, 0x04, 0x00, 0xF0, 0xF1, 0xF1, 0xF2,
    0xF2, 0xF4, 0xF4
  };


static Byte charsets[] = {
    0x81, 0x85, 0xf2, 0x00, 0x09, 0x10, 0x70, 0x00,
    0x00, 0x00, 0x07, 
    /*
     * Base character set descriptors: the last two bytes are the code
     * page.  It must match the tables we draw and type with (asc_ebc.c):
     * CP037.  It used to say 0x0129 (CP297, French), so hosts sent French
     * EBCDIC and "!" showed as "|", "\" as "$", and so on.
     */
#ifndef FRENCH
    0x00, 0x00, 0x00, 0x02, 0xaa, 0x00, 0x25,
    0x00, 0x00, 0x00, 0x02, 0xb9, 0x00, 0x25,
#else
    0x00, 0x00, 0x00, 0x02, 0xaa, 0x01, 0x29,
    0x00, 0x00, 0x00, 0x02, 0xb9, 0x01, 0x29,
#endif
    0x01, 0x00, 0xf1, 0x03, 0xc3, 0x01, 0x36,
    0x02, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
  };
/* static Byte charsets[] = {   test en cours pour PS  PV
    0x81, 0x85, 0x80, 0x00, 0x06, 0x0C, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00,
    0xF1
  };                                   */
/* mychar
static Byte charsets[] = {
    0x81, 0x85, 0x80, 0x00, 0x09, 0x10, 0x70, 0x00,
    0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00,
    0xF1
  };
*/

/* not really sure about correctness of the following..
 */
fs_resize(width, height, auawidth, auaheight, lines, columns)
{
    
    /* junk 
       uarea[19 - 2] = width;
       uarea[20 - 2] = height;
       uarea[44 - 2] = auawidth;
       uarea[45 - 2] = auaheight;
       charsets[6 - 2] = width;
       charsets[7 - 2] = width;
       implpart[16 -2] = lines;
       implpart[20 -2] = width;
       implpart[22 -2] = height;
       */
}


static Byte nullreply[] = {
    0x81, 0xff
  };

struct attr att[] =  {
    { ISF_QR_NULL,                0,  4, nullreply}, 
    { ISF_QR_COLOR,         DEFAULT, 22, color},
    { ISF_QR_HIGHLIGHT,     DEFAULT, 13, highlight}, 
    { ISF_QR_CHARSETS,      DEFAULT, 48, charsets},
    { ISF_QR_UAREA,         DEFAULT, 46, uarea},
    { ISF_QR_IMPLICIT_PART, DEFAULT, 28, implpart},
    { ISF_QR_REPLY_MODE,    DEFAULT, 7,  replymode},
    { ISF_QR_SUMMARY,       DEFAULT, 22, summary},
    { ISF_QR_ALPHA_PART,    DEFAULT, 8,  alphapart},
    { ISF_QR_STORAGE_POOLS,       0, 17, storagepool},
    { ISF_QR_AUX_DEV,       DEFAULT, 6,  auxdev},
    { ISF_QR_RPQNAMES,            0, 18, rpqnames},
    { ISF_QR_BACKGRND_TRANSP,     0, 9,  backgrdtransp},
#if 0/* funny length in mactn3270 */
    { ISF_QR_SEGMENT,             0, 26, segment},
#else
    { ISF_QR_SEGMENT,             0, 21, segment},
#endif
    { ISF_QR_PROCEDURE,           0, 21, procedure},
    { ISF_QR_LINE_TYPE,           0, 24, linetype},
    { ISF_QR_PORT,                0, 17, port1}, /* pv ne pas supprimer le curseur*/
    { ISF_QR_PORT,                0, 15, port2},
    { ISF_QR_GRAPHIC_COLOR,       0, 61, graphiccolor},
    { ISF_QR_GRAPHIC_SYM_SET,     0,133, graphicsymset},
};


/* select the appropriate IBM 3270 to emulate based on the 
 * screen_lines and screen_columns.
 */
fs_select_term ()
{

    static struct termtype	terminfo[] = {
	27,	132,	24,	80,	"IBM-3279-5-E",
	43,	80,	24,	80,	"IBM-3279-4-E",
#ifndef MONOCOL_TEST
	32,     80,     24,     80,     "IBM-3279-3-E",
	24,	80,	24,	80,	"IBM-3279-2-E",
#else
	32,	80,	24,	80,	"IBM-3278-3",
	24,	80,	24,	80,	"IBM-3278-2",
#endif
	12,	80,	12,	40,	"IBM-3278-1",
	12,	40,	12,	40,	"IBM-3277-1",
	-1,	-1,	-1,	-1,	(char *)NULL
      };
    register struct termtype	*tp = terminfo;
    
    DBUG_ENTER("fs_select_term");
    
    while (tp->rows > screen_lines || tp->cols > screen_columns)
      tp++;
    maxlines = tp->rows;
    maxlinelength = tp->cols;
    lines = deflines = tp->defrows;
    linelength = deflinelength = tp->defcols;
    ibm_term = tp->name;
    uarea[UROW_INDEX1] = uarea[UROW_INDEX2] = tp->rows;
    uarea[UCOL_INDEX1] = uarea[UCOL_INDEX2] = tp->cols;
    implpart[IROW_INDEX1] = implpart[IROW_INDEX2] = tp->rows;
    implpart[ICOL_INDEX1] = implpart[ICOL_INDEX2] = tp->cols;
    implpart[23] = char_width                     ;
    implpart[25] = char_height                    ;
    DBUG_PRINT("term",("i am \"%s\", he gets \"%s\"\n", real_term, ibm_term));
    DBUG_RETURN(1);
}

/* sound the terminal alarm */

 fs_alarm ()
{
  Bell();
}


fs_display_shift(on)
     int on;
{
    status_disp (41, on ? CG_UPSHIFT : CG_BLANK);
}

int fs_backtab ()
{
  register int	baddr, nbaddr;
  int			sbaddr;
  
  if (keyboard_locked)
    return;
  baddr = cursor_addr;
  DEC_BA (baddr);
  if (IS_ATTR (baddr))	/* at bof */
    DEC_BA (baddr);
  sbaddr = baddr;
  while (TRUE) {
    nbaddr = baddr;
    INC_BA (nbaddr);
    if (IS_ATTR(baddr)
	&&  !IS_PROTECTED (baddr)
	&&  !IS_ATTR (nbaddr))
      break;
    DEC_BA (baddr);
    if (baddr == sbaddr) {
      cursor_move (0);
      return;
    }
  }
  INC_BA (baddr);
  cursor_move (baddr);
}

/*
 * Find the field attribute for the given buffer address.
 */

int 
  get_field_attribute (baddr)
register int	baddr;
{
  static Attr	fake_fa;
  int		sbaddr;
  
  DBUG_ENTER("get_field_attribute");
  
  sbaddr = baddr;
  do {
    if (IS_ATTR(baddr)) 
      {
	DBUG_PRINT("getfa",("found attribute for addresss %d (r=%d c=%d) at %d (r=%d c=%d), attr=0x%x\n",
			    sbaddr, ADDR_TO_R(sbaddr),ADDR_TO_C(sbaddr), baddr,ADDR_TO_R(baddr),
			    ADDR_TO_C(baddr), atrbuf[baddr]));
	DBUG_RETURN (baddr);
      }
    DEC_BA (baddr);
  } while (baddr != sbaddr);
  
  DBUG_PRINT("getfa",("PANIC-COULDNT FIND attribute for addresss %s\n",sbaddr));
  assert(1);
  DBUG_RETURN(0);
}


/*
 * The EBCDIC code for a typed or pasted character.  ch is a Latin-1 code
 * (0..255) or one of the negative DO_DUP / DO_FIELDMARK pseudo keys,
 * which have their own EBCDIC control codes (atoe[] only covers 0..255).
 */
static int char_to_ebcdic (ch)
     int	ch;
{
  if (ch == DO_DUP)
    return EBCDIC_DUP;
  if (ch == DO_FIELDMARK)
    return EBCDIC_FM;
  return atoe[ch & 0xff];
}

int fs_character (ch)
     int	ch;
{
  register int	baddr, end_baddr, t_baddr;
  Attr  fa_index;
  
  DBUG_ENTER("fs_character");
  
  if (keyboard_locked)
    DBUG_VOID_RETURN;
  
  
  baddr = cursor_addr;

  
  DBUG_PRINT("term",("char `%c', address=%d, attr=0x%x\n",ch,baddr,atrbuf[baddr]));
  
  if (IS_ATTR (baddr) || IS_PROTECTED (baddr)) {
    status_disp (8, CG_LOCK);
    status_disp (9, CG_BLANK);
    status_disp (10, CG_LEFTARROW);
    status_disp (11, CG_HUMAN);
    status_disp (12, CG_RIGHTARROW);
    keyboard_locked = TRUE;
    DBUG_VOID_RETURN;
  }
  
  if (IS_NUMERIC (baddr)
      &&  !((isdigit(ch) || (ch == '.'))))
    {
      status_disp (8, CG_LOCK);
      status_disp (9, CG_BLANK);
      status_disp (10, CG_HUMAN);
      status_disp (11, CG_CN);
      status_disp (12, CG_CU);
      status_disp (13, CG_CM);
      keyboard_locked = TRUE;
      DBUG_VOID_RETURN;
    }



  if (insert && (chrbuf[baddr] != EBCDIC_NUL)) { /* DUBIOUS NULL TEST -mah */
    /* find next null or next fa */
    end_baddr = baddr;
    do {
      INC_BA (end_baddr);
      if ((chrbuf[end_baddr] == EBCDIC_NUL)
	  ||  IS_ATTR (end_baddr))
	break;
    } while (end_baddr != baddr);
    if (chrbuf[end_baddr] != EBCDIC_NUL) {
      status_disp (8, CG_LOCK);
      status_disp (9, CG_BLANK);
      status_disp (10, CG_HUMAN);
      status_disp (11, CG_GREATER);
      keyboard_locked = TRUE;
      DBUG_VOID_RETURN;
      
    }
    else {
      /* was ist mit modified fields hier? */
      if (end_baddr > baddr)
	bcopy ((char *) &chrbuf[baddr], 
	       (char *) &chrbuf[baddr+1], 
	       end_baddr - baddr);
      else {
	bcopy ((char *) &chrbuf[0], 
	       (char *) &chrbuf[1], end_baddr);
	
	chrbuf[0] = chrbuf[(screen_size) - 1];
	
	bcopy ((char *) &chrbuf[baddr], 
	       (char *) &chrbuf[baddr+1], 
	       ((screen_size) - 1) - baddr);
      }
      chrbuf[baddr] = char_to_ebcdic(ch);
      atrbuf[baddr] |= SATTR_MDT;
      atrbuf[baddr] = atrbuf[get_field_attribute(baddr)]  & ~SATTR_ATTRIB
	|  SATTR_MDT;
      cursor_off ();
      t_baddr = baddr;
      while (t_baddr != end_baddr) {
	screen_update (t_baddr);
	INC_BA (t_baddr);
      }
      screen_update (t_baddr);
    }
  }
  else {
    chrbuf[baddr] = char_to_ebcdic(ch);
    atrbuf[baddr] = atrbuf[get_field_attribute(baddr)]  & ~SATTR_ATTRIB
		|  SATTR_MDT;
    cursor_off ();
    screen_update (baddr);
  }
  atrbuf[get_field_attribute(baddr)] |= SATTR_MDT;

/*   atrbuf[baddr] |= SATTR_MDT;	/* was atrbuf[get_field_attribute(baddr)] */

  INC_BA (baddr);
  if (IS_ATTR (baddr)) {
DBUG_PRINT("cursor",("AUTOSKIP: field attr =0x%lx\n",atrbuf[baddr]));
    if (IS_NUMERIC (baddr)) {
      if (IS_PROTECTED (baddr)) {
	/* skip to next unprotected */
	while (TRUE) {
	  INC_BA (baddr);
	  if (IS_ATTR (baddr)
	      &&  !IS_PROTECTED (baddr))
	    break;
	}
	INC_BA (baddr);
      }
      else
	INC_BA (baddr);
    }
    else
      INC_BA (baddr);
  }
  cursor_move (baddr);
  cursor_on ();
  
  
  DBUG_VOID_RETURN;
}

int fs_clear ()
{
    clear_screen_buffer ();
 /*   lines = deflines;            sur un vrai le clear ne change pas la
      linelength = deflinelength;  taille de l'ecran         */
    send_pa (AID_CLEAR);
}

int fs_delete ()
{
    register int	baddr, end_baddr, t_baddr;

    if (keyboard_locked)
	return;
    baddr = cursor_addr;
    if (IS_PROTECTED (baddr) || IS_ATTR(baddr)) {
	status_disp (8, CG_LOCK);
	status_disp (9, CG_BLANK);
	status_disp (10, CG_LEFTARROW);
	status_disp (11, CG_HUMAN);
	status_disp (12, CG_RIGHTARROW);
	keyboard_locked = TRUE;
    }
    else {
	/* find next fa */
	end_baddr = baddr;
	do {
	    INC_BA (end_baddr);
	    if (IS_ATTR (end_baddr))
		break;
	} while (end_baddr != baddr);
	DEC_BA (end_baddr);
	if (end_baddr > baddr)
	    bcopy ((char *) &chrbuf[baddr+1], 
		   (char *) &chrbuf[baddr], end_baddr - baddr);
	else {
	    bcopy ((char *) &chrbuf[baddr+1], 
		   (char *) &chrbuf[baddr], (screen_size - 1) - baddr);
	    chrbuf[screen_size - 1] = chrbuf[0];
	    bcopy ((char *) &chrbuf[1], (char *) &chrbuf[0], end_baddr);
	}
	chrbuf[end_baddr] = EBCDIC_NUL;
	cursor_off ();
	t_baddr = baddr;
	atrbuf[get_field_attribute(baddr)] |= SATTR_MDT;
	while (t_baddr != end_baddr) {
	    screen_update (t_baddr);
	    atrbuf[t_baddr] |= SATTR_MDT;
	    INC_BA (t_baddr);
	}
	screen_update (t_baddr);
	cursor_on ();
    }

}

int fs_down ()
{
    register int	baddr;

    if (keyboard_locked)
	return;
    baddr = (cursor_addr + screen_columns) % screen_size;
    cursor_move (baddr);
}

int fs_dup ()
{
    fs_character (DO_DUP);
}

int fs_enter ()
{
    send_modified_fields (AID_ENTER);
}

int fs_eraseeof ()
{
    register int	baddr;

    if (keyboard_locked)
	return;
    baddr = cursor_addr;
    if (IS_PROTECTED (baddr) || IS_ATTR(baddr)) {
	status_disp (8, CG_LOCK);
	status_disp (9, CG_BLANK);
	status_disp (10, CG_LEFTARROW);
	status_disp (11, CG_HUMAN);
	status_disp (12, CG_RIGHTARROW);
	keyboard_locked = TRUE;
    }
    else {
	if (formatted) {	/* erase to next field attribute */
	    cursor_off ();
	    do {
		chrbuf[baddr] = EBCDIC_NUL;
		atrbuf[baddr] |= SATTR_MDT;
		screen_update (baddr);
		INC_BA (baddr);
	    } while (!IS_ATTR (baddr));
	    cursor_on ();
	}
	else {	/* erase to end of screen */
	    cursor_off ();
	    do {
		chrbuf[baddr] = EBCDIC_NUL;
		screen_update (baddr);
		INC_BA (baddr);
	    } while (baddr != 0);
	    cursor_on ();
	}
    }
}

int fs_eraseinput ()
{
    erase_all_unprotected ();
}

int fs_fieldmark ()
{
    fs_character (DO_FIELDMARK);
}

/*
 * Move to first unprotected field on screen.
 */
fs_home ()
{
    register int	baddr, nbaddr;
    register u_char	*fa;

    if (keyboard_locked)
	return;
    if (!IS_PROTECTED (0))
	cursor_move (0);
    else {
	nbaddr = 1;		/* start at 2nd col, 1st col is fa */
	while (TRUE) {
	    baddr = nbaddr;
	    INC_BA (nbaddr);
	    if (IS_ATTR (baddr)
	    &&  !IS_PROTECTED (baddr)
	    &&  !IS_ATTR(nbaddr))
		break;
	    if (baddr == 0) {
		cursor_move (0);
		return;
	    }
	}
	INC_BA (baddr);
	cursor_move (baddr);
    }
}

int fs_insertmode ()
{
    insert_mode(TRUE);
}

int fs_left ()
{
    register int	baddr;

    if (keyboard_locked)
	return;
    baddr = cursor_addr;
    DEC_BA (baddr);
    cursor_move (baddr);

}

int fs_pa1 ()
{
    send_pa (AID_PA1);
}

int fs_pa2 ()
{
    send_pa (AID_PA2);
}

int fs_pa3 ()
{
    send_pa (AID_PA3);
}

int fs_penselect ()
{
    fprintf (stderr, "\rSorry, no pen select yet\r");
}

int fs_pf1 ()
{
    send_modified_fields (AID_PF1);
}

int fs_pf10 ()
{
    send_modified_fields (AID_PF10);
}

int fs_pf11 ()
{
    send_modified_fields (AID_PF11);
}

int fs_pf12 ()
{
    send_modified_fields (AID_PF12);
}

int fs_pf13 ()
{
    send_modified_fields (AID_PF13);
}

int fs_pf14 ()
{
    send_modified_fields (AID_PF14);
}

int fs_pf15 ()
{
    send_modified_fields (AID_PF15);
}

int fs_pf16 ()
{
    send_modified_fields (AID_PF16);
}

int fs_pf17 ()
{
    send_modified_fields (AID_PF17);
}

int fs_pf18 ()
{
    send_modified_fields (AID_PF18);
}

int fs_pf19 ()
{
    send_modified_fields (AID_PF19);
}

int fs_pf2 ()
{
    send_modified_fields (AID_PF2);
}

int fs_pf20 ()
{
    send_modified_fields (AID_PF20);
}

int fs_pf21 ()
{
    send_modified_fields (AID_PF21);
}

int fs_pf22 ()
{
    send_modified_fields (AID_PF22);
}

int fs_pf23 ()
{
    send_modified_fields (AID_PF23);
}

int fs_pf24 ()
{
    send_modified_fields (AID_PF24);
}

int fs_pf3 ()
{
    send_modified_fields (AID_PF3);
}

int fs_pf4 ()
{
    send_modified_fields (AID_PF4);
}

int fs_pf5 ()
{
    send_modified_fields (AID_PF5);
}

int fs_pf6 ()
{
    send_modified_fields (AID_PF6);
}

int fs_pf7 ()
{
    send_modified_fields (AID_PF7);
}

int fs_pf8 ()
{
    send_modified_fields (AID_PF8);
}

int fs_pf9 ()
{
    send_modified_fields (AID_PF9);
}
/*
 * Handle an AID (Attention IDentifier) key.  This is the common stuff that
 * gets executed for all AID keys (PFs, PAs, Clear and etc).
 */

key_AID ()
{
    status_disp (1, CG_BLANK);
    status_disp (8, CG_LOCK);
    status_disp (9, CG_BLANK);
    status_disp (10, CG_CLOCKLEFT);
    status_disp (11, CG_CLOCKRIGHT);
    insert_mode (FALSE);
    keyboard_locked = TRUE;
    status_disp (1, CG_UNDERA);
}


/*
 * Toggle insert mode.
 */
insert_mode (on)
Bool	on;
{
    if (on) {
	insert = TRUE;
	status_disp (51, CG_INSERT);
    }
    else {
	insert = FALSE;
	status_disp (51, CG_BLANK);
    }
}

/*
 * Reset keyboard lock.
 */
fs_reset ()
{
    register int	i;

    keyboard_locked = FALSE;
    insert_mode(FALSE);
    for (i = 0; i < 9; i++)
	status_disp (i + 8, CG_BLANK);
}


/*FIXME*/
fs_mono_case()
{
}

static int blink;

fs_cursor_blink()
{
  blink = !blink;
  CursorBlink(blink);
}

static int alt;
fs_cursor_alt()
{
  alt = !alt;
  CursorAlt(alt);
}

/*
 * Backspace: erase the character left of the cursor and move the
 * cursor onto it (3270 "Erase" semantics).  The rest of the field
 * shifts left by one, via fs_delete(), which also sets the MDT so
 * the change is sent to the host.
 *
 * Nothing happens at the first position of a field (the position to
 * the left is the field's attribute byte) or outside an input field,
 * so the cursor can never back into a protected area.
 */
fs_backspace()
{
    register int	baddr;

    if (keyboard_locked)
	return;
    baddr = cursor_addr;
    if (IS_ATTR(baddr) || IS_PROTECTED(baddr))
	return;			/* not in an input field */
    DEC_BA (baddr);
    if (IS_ATTR(baddr))
	return;			/* already at start of field */
    cursor_move (baddr);
    fs_delete ();
}

/**/


/* act like a 3270 "Local Newline" key */

int fs_return ()
{
    register int	baddr;
    Attr fa;

    if (keyboard_locked)
	return;
    baddr = (cursor_addr + screen_columns) % (screen_size);	/* down */
    baddr = (baddr / screen_columns) * screen_columns ;	        /* 1st col */

    if (!IS_ATTR(baddr) && !IS_PROTECTED (baddr)) /*  THE NEXT TWO TESTS ARE DUBIOIS */
	cursor_move (baddr);
    else {	/* find next unprotected */
	if (!IS_ATTR(baddr) && !IS_PROTECTED (baddr)) /* CHECK MANUAL -FIXME*/
	  {
	    INC_BA (baddr);
	  }
	else {
	    while (TRUE) {
		INC_BA (baddr);
		if (IS_ATTR (baddr)
		&&  !IS_PROTECTED (baddr))
		    break;
		if (baddr == cursor_addr) {
		    cursor_move (0);
		    return;
		}
	    }
	    INC_BA (baddr);
	}
	cursor_move (baddr);
    }
}

int fs_sysreq ()
{
  if (keyboard_locked)
    return;

  send_modified_fields (AID_SYSREQ);
  lines = deflines;
  linelength = deflinelength;
}

/* act like a 3270 "Local Tab" key */
/*
 * Tab forward to next field.
 */
fs_tab ()
{
    register int	baddr, nbaddr;

    if (keyboard_locked)
	return;
    nbaddr = cursor_addr;
    INC_BA (nbaddr);
    while (TRUE) {
	baddr = nbaddr;
	INC_BA (nbaddr);
	if (IS_ATTR(baddr)
	&&  !IS_PROTECTED (baddr)
	&&  !IS_ATTR (nbaddr))
	    break;
	if (baddr == cursor_addr) {
	    cursor_move (0);
	    return;
	}
    }
    INC_BA (baddr);
    cursor_move (baddr);
}

int fs_right ()
{
  register int	baddr;

  if (keyboard_locked)
    return;
  baddr = cursor_addr;
  INC_BA (baddr);
  cursor_move (baddr);

}

int fs_up ()
{
    register int	baddr;

    if (keyboard_locked)
	return;
    baddr = cursor_addr - screen_columns;
    if (baddr < 0)
	baddr = (cursor_addr + (screen_size)) - screen_columns;
    cursor_move (baddr);
}

fs_int_data(ch,ge_flag)
     Byte ch;
     int ge_flag;		/* graphics escape */
{

  chrbuf[buffer_addr] = ch;
  atrbuf[buffer_addr] = current_attr ;

  if (ge_flag || 
      (((ch == 0xad) || (ch == 0xbd)) && fixbracket))
    atrbuf[buffer_addr] |= SATTR_GRXESC;
  screen_update(buffer_addr);
}


/*
 *  Set Attribute Order
 *     tptr points to attribute type following ORDER_SA
 *
 *  Side effect: sets current attribute. This is used until another
 *     write command, or the next Set Attribute order.
 */
fs_int_sa(tptr)
     Byte *tptr;
{
    DBUG_ENTER("fs_int_sa");
    if ((*tptr == 0) && (*(tptr + 1) == 0)) {
	current_attr = defattr;
	current_attr_ra = current_attr ;
	DBUG_VOID_RETURN;
    }
    current_attr = fs_int_attr(current_attr, tptr, 1);
    current_attr &= ~SATTR_ATTRIB;
    current_attr_ra = current_attr ;
    if ((current_attr_ra & 0x0f) == 0) current_attr = current_attr
      + (atrbuf[get_field_attribute(buffer_addr)] & 0x0f & ~SATTR_ATTRIB);
    if ((current_attr_ra & 0xf0) == 0) current_attr = current_attr
      + (atrbuf[get_field_attribute(buffer_addr)] & 0xf0 & ~SATTR_ATTRIB);

    DBUG_VOID_RETURN;
}

/* field points to first type byte;
 *
 */
fs_int_attr(atr, field, pairs)
     Attr atr;
     Byte  *field;
     Byte pairs;
{
    register Byte type;
    register int offset = 0;
    register Attr c;
    
    DBUG_ENTER("fs_int_attr");
    DBUG_PRINT("attr", ("%d attribute pairs\n",pairs));
    
    while (pairs > 0) {
	
	type = field[offset];	/* type of extended field attribute */
	c = (Attr) field[offset + 1];	/* value */
	DBUG_PRINT("attr", ("do type 0x%x value 0x%x\n",type, c));
	
	switch(type) {
	  case EFA_RESET :
	    DBUG_PRINT("attr", ("efa reset\n"));
	    atr = 0;
	    break;
	    
	  case EFA_FIELD :
	    atr &= ~SATTR_MASK;
	    atr |= (c & ~ATTR_TOP) << 8;
	    DBUG_PRINT("attr", ("efa plain field attribute 0x%x, attr now=0x%x, c<<8=0x%x\n", 
				c & ~ATTR_TOP,atr, (c & ~ATTR_TOP)<<8));
	    break;
	    
	  case EFA_EXT_HIGH:
	    DBUG_PRINT("attr", ("efa ext highlight 0x%x\n", c));
	    atr&= ~SATTR_DISPLAYMASK;
	    atr |= ((c & 0x07) << 4) ;/* maybe map blinking here to */
	     break;                      /* some other attribute */

	  case EFA_COLOR:
	    DBUG_PRINT("attr", ("efa color 0x%x\n", c ));
	    atr = atr - (atr & 0x0f) + (c & 0x0f);   /*   pv  */
	    colorfield = 1;
	    break;
	    
	  case EFA_P_SYM:
	    DBUG_PRINT("attr", ("efa symbol set 0x%x\n", c ));
	    atr &= ~SATTR_CS_MASK;
	    atr |=  ((c << 16) & SATTR_CS_MASK);
	    break;
	    
	  case EFA_BG_TRANSP:
	    DBUG_PRINT("attr", ("efa background transparency 0x%x\n", c ));
	    atr |= (c & SATTR_T_INTRANS);   /* if c == ff, set background */
	    break;				/* intransparent  */
	    
	  default:
	    DBUG_PRINT("attr", ("efa UNKNOWN TYPE  0x%x\n", type ));
	    break;
	}
	offset += 2;
	pairs--;
    }
    DBUG_PRINT("attr", ("attr(0x%x)=\"%s\"\n", atr,decode_attr(atr)));
    DBUG_RETURN(atr);
}

/* tell just about anything about the attr field */
static char *
  decode_attr(attr)
Attr attr;
{
    static char buf[300];
    static struct attinfo {
	int mask;
	char *name;
    } attcvt[] = {
	{ SATTR_ATTRIB, "ATT "},
	{ SATTR_D_BLINK, "blink "},
	{ SATTR_D_REVERSE, "reverse " },
	{ SATTR_D_UNDERLINE, "underline "},
	{ SATTR_T_INTRANS, "bg-intransp "},
	{ SATTR_GRXESC, "gxesc "},
	{ SATTR_PROT, "prot " },
	{ SATTR_NUMERIC, "num " },
	{ SATTR_CS_APL, "APL "},
	{ SATTR_LP, "lo/pen "},
	{ SATTR_HP, "hi/pen "},
	{ SATTR_IN, "invis "},
	{ SATTR_MDT, "MOD "},
	{ 0, NULL }};
    static char *colors[] = {
	"Default", "Blue", "Red", "Pink", "Green",
	"Turq", "Yell", "White", 
	"?f8?","?f9?","?fa?","?fb?","?fc?","?fd?","?fe?","?ff?"
    };
    
    struct attinfo *ap = attcvt;
    buf[0] = 0;
    
    while (ap->mask != 0) {
	if ((ap->mask & attr) == ap->mask)
	  strcat(buf, ap->name);
	ap++;
    }
    strcat(buf, colors[attr & SATTR_COLORMASK]);
    return buf;
}


/*
 * Read Partition Query 
 * reverse engineered from MacTn3270
 */
int
  fs_read_partition_query(len, query)
int len;
Byte *query;
{
    int i, j, listlen, reqtype, t, typecount;
    int sentfield = 0;
    
    DBUG_ENTER("fs_read_partition_query");
    
    switch (query[4]) {
      case 0x02:				/* Query */
	listlen = 0;
	reqtype = 1;
	break;
      case 0x03:				/* Query List */
	if (len < 6) return;
	listlen = len - 6;
	reqtype = (query[5] & 0xc0) >> 6;
	if (reqtype == 3) 
	  return;
	break;
      default:
	DBUG_VOID_RETURN;
    }
    
    net_add_ch(AID_ISF);               
    
    typecount = sizeof(att)/sizeof(att[0])    ; /* skip NULL response for now */
    
    /*  loop through all possible responses;
     *  include in response those requested
     */
    for (i = 1; i < typecount; i++) {	
	t = att[i].type;
	if (reqtype == 2) {		        /* all types */
	    fs_add_sf(i);
	    sentfield++;
	    continue;
	}
	if ((reqtype == 1) && 
	    (att[i].flags & DEFAULT)) {  /* default types wanted */
	    fs_add_sf(i);
	    sentfield++;
	    continue;
	}
	if (listlen == 0) 
	  continue;		                 /* listed types wanted */
	for (j=0; j < listlen; j++)
	 if (query[j+6] == t) {
	      fs_add_sf(i);
	      sentfield++;
	      break;
	  }
    }
    if (!sentfield)               /* return null response if no other */
      fs_add_sf(0);
    
    net_add_eor();		/* finishing touch */
    last_aid = AID_ISF;
    
    DBUG_VOID_RETURN;
}

/* add structured field from attr[ind]
 * to response 
 */
static void
  fs_add_sf(ind)
int ind;
{
    register int n;
    register struct attr *ap;
    
    DBUG_ENTER("fs_add_sf");
    
    ap = &att[ind];
    
    /* add length of structured field */
    net_add_ch((ap->length >> 8) & 0x00ff);
    net_add_ch(ap->length & 0x00ff);
    
    /* actual contents */
    net_add_buf(ap->value,ap->length - 2);
    
    DBUG_PRINT("rpq",("adding SF type 0x%x, length %d\n", 
		      ap->value[1], ap->length));
    DBUG_VOID_RETURN;
}




static char *
  strcmd(cmd)
Byte cmd;
{
    static struct cmdcvt {
	Byte op;
	char *name;
    } cmds[] = {
	{ CMD_EAU,	"Erase All Unprotected" },
	{ CMD_EW,	"Erase Write" },
	{ CMD_EWA,	"Erase Write Alternate" },
	{ CMD_RB,	"Read Buffer" },
	{ CMD_RM,	"Read Modified" },
	{ CMD_WRITE,	"Write" },
	{ CMD_NOOP,	"NOOP" },
	{ CMD_SEL,	"SEL" },
	{ CMD_SENSE,	"SENSE" },
	{ CMD_SID,	"SID" },
	{ CMD_SNA_EAU,	"SNA Erase all unprotected" },
	{ CMD_SNA_EW,	"SNA Erase Write" },
	{ CMD_SNA_EWA,	"SNA Erase Write Alternate" },
	{ CMD_SNA_RB,	"SNA Read Buffer" },
	{ CMD_SNA_RM,	"SNA Read Modified" },
	{ CMD_SNA_RMA,	"SNA Read Modified All" },
	{ CMD_SNA_WRITE,"SNA Write" },
	{ CMD_WSF,	"Write Structured Field" },
	{ CMD_MAGIC,	"MAGIC-Cookie, like Write STructured Field" },
	{ 0, NULL}
    };
    static char buf[30];
    
    struct cmdcvt *cp = cmds;
    
    while (cp->op != 0) {
	if (cp->op == cmd) 
	  return cp->name;
	cp++;
    }
    
    sprintf(buf,"*** Unknown cmd ** 0x%x",cmd);
    return buf;
}


clear_window()
{
  ClearScreen();
  has_graphics = 0;
}


static void fillblank(s, l)
     char *s;
     int l;

{
  int i;

  for (i = 0; i < l; i++, s++){
    *s = def_char;
  }
}


/* called when the physical screen is resized 
 * (from x3270.c)
 */
fs_set_screensize(lines, cols)
     int lines, cols;
{
    screen_lines = lines;
    screen_columns = cols;
    screen_size = lines * cols;
}


/* This array contains a one for each visible ebcdic character,
   and zero for attribute characters and space.
 */
static char 
visible_char[256] = {
    /*   0 */ 0, 0, 0, 0, 0, 0, 0, 0, 
    /*   8 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  16 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  24 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  32 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  40 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  48 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  56 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  64 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  72 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  80 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  88 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /*  96 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 104 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 112 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 120 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 128 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 136 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 144 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 152 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 160 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 168 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 176 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 184 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 192 */ 0, 0, 0, 0, 0, 0, 0, 0, 
    /* 200 */ 0, 0, 0, 0, 0, 0, 0, 0, 
    /* 208 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 216 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 224 */ 0, 0, 0, 0, 0, 0, 0, 0, 
    /* 232 */ 0, 0, 0, 0, 0, 0, 0, 0, 
    /* 240 */ 1, 1, 1, 1, 1, 1, 1, 1, 
    /* 248 */ 1, 1, 1, 1, 1, 1, 1, 1
    };

static int x1, y1, x2, y2;   /* global, initialized by init, used by next_item */
static int curr_x, curr_y,curr_pos;


int init(lx, ly, rx, ry)
     int lx, ly, rx, ry;
{
  int ind;

  DBUG_ENTER("init");

  x1 = lx;
  y1 = ly;
  x2 = rx;
  y2 = ry;
 
  ind = y1 * screen_columns + x1;
  curr_pos = ind;
  curr_x = x1;
  curr_y = y1;
  DBUG_PRINT("init",("init iterator x0=%d y0=%d x1=%d y1=%d\n",lx,ly,rx,ry));
  DBUG_RETURN(1);
}


int next_item(x, y, s, attribs,color)
     int *x,*y;
     char *s;
     Attr *attribs;
     int  *color;
{
  char field[132];

  int i = 0, indx;
  char *start = s;
  

  /* for each line in the rectangle */
  while (curr_y <= y2){

    curr_pos = curr_y * screen_columns + curr_x;

    /*
     * or curr_pos points to the first interesting character after
     * the last field but in the same line.
     * Collect visible characters within the rectangle having the
     * same attributes.
     */

    *x = curr_x;		
    *y = curr_y;
    *attribs = disp_attr(curr_pos);	/* selection shown on expose too */
    *color = convcol(*attribs) & SATTR_COLORMASK;

    while ((curr_x < x2) &&
	   (*attribs == disp_attr(curr_pos)))
      {
	*s++ = chrbuf[curr_pos]; 
	curr_pos++;		
	curr_x++; 
      }
    
    /* if we're done with this line, skip */
    if (curr_x == x2)
      {
	curr_x = x1;
	curr_y++;
      }
    /* fprintf(stderr, "length=%d\n", s - start);
    /* assert((s - start) > 0); */

    return (s - start);
  }
  return 0;
}
    







