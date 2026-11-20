/*
 *  The following definitions were taken from:
 *	"IBM 3270 Information Display System Component Description",
 *	GA27-2749-10	(11th edition, February 1980)
 *
 *  Extensions for graphics/SNA taken from "3270 Information Display System
 *  Data Stream Programmers Reference", GA23-0059-05, 6th Ed. March 1990
 *  Mon May 14 14:40:53 1990 -- Michael Haberler mah@awiwuw11.bitnet
 *
 *  Caveat: The IBM bit numbering conventions are uh, nonstandard.
 *  The 0x80 bit in a byte is called `Bit 0', the 0x01 bit is `Bit 7'.
 *
 */
 
/* Special Characters */
 
# define	EBCDIC_NUL	0x00	/* Null Character */
# define	EBCDIC_SOH	0x01	/* Start Of Header */
# define	EBCDIC_STX	0x02	/* Start Of Text */
# define	EBCDIC_GE	0x08	/* Graphics Escape */
# define	EBCDIC_DUP	0x1c	/* DUP code */
# define	EBCDIC_FM	0x1e	/* Field Mark code */
# define	EBCDIC_SP	0x40	/* Space */
 
/* Command Codes (3272 & 3274-1B/D version) */
 
# define	CMD_EAU		0x0f	/* Erase All Unprotected */
# define	CMD_EW		0x05	/* Erase/Write */
# define	CMD_EWA		0x0d	/* Erase/Write Alternate */
# define	CMD_RB		0x02	/* Read Buffer */
# define	CMD_RM		0x06	/* Read Modified */
# define	CMD_WRITE	0x01	/* Write */
# define	CMD_NOOP	0x03	/* No Operation */
# define	CMD_SEL		0x0b	/* Select */
# define	CMD_SENSE	0x04	/* Sense */
# define	CMD_SID		0xe4	/* Sense ID */

# define	CMD_SNA_EAU	0x6f	/* SNA Erase All Unprotected */
# define	CMD_SNA_EW	0xf5	/* SNA Erase/Write */
# define	CMD_SNA_EWA	0x7e	/* SNA Erase/Write Alternate */
# define	CMD_SNA_RB	0xf2	/* SNA Read Buffer */
# define	CMD_SNA_RM	0xf6	/* SNA Read Modified */
# define	CMD_SNA_RMA	0x6e	/* SNA Read Modified All */
# define	CMD_SNA_WRITE	0xf1	/* SNA Write */

# define        CMD_WSF         0xf3    /* Write Structured Field */
# define        CMD_MAGIC       0x11    /* handled in MacTn3270 like CMD_WSF */
                                        /* couldnt find this in manual (hints?) */
 
/* Orders in a Write stream */
 
# define	ORDER_SF	0x1d	/* Start Field */
# define	ORDER_SBA	0x11	/* Set Buffer Address */
# define	ORDER_IC	0x13	/* Insert Cursor */
# define	ORDER_PT	0x05	/* Program Tab */
# define	ORDER_RA	0x3c	/* Repeat to Address */
# define	ORDER_EUA	0x12	/* Erase Unprotected to Address */ 

# define        ORDER_SFE       0x29    /* Start Field Extended */
# define        ORDER_MF        0x2c    /* Modify Field */
# define        ORDER_SA        0x28    /* Set Attribute */
# define        ORDER_GE        0x08    /* Graphics Escape */


/* ID's for Outbound Structured Fields */

# define        OSF_ACTIVATE_PARTITION    0x0e
# define        OSF_BEGIN_END_FILE        0x0f85
# define        OSF_CREATE_PARTITION      0x0c
# define        OSF_DESTROY_PARTITION     0x0d
# define        OSF_ERASE_RESET           0x03
# define        OSF_LOAD_COLOR_TABLE      0x0f05
# define        OSF_LOAD_FORMAT_STORAGE   0x0f24
# define        OSF_LOAD_LINE_TYPE        0x0f07
# define        OSF_LOAD_PROG_SYMBOL      0x06
# define        OSF_MOIFIY_PARTITION      0x0f0a
# define        OSF_OUTBOUND_TEXT_HEADER  0x0f71
# define        OSF_OUTBOUND_TYPE_1_TEXT  0x0fc1
# define        OSF_OUTBOUND_3270DS       0x40
# define        OSF_PRESENT_ABS_FMT       0x4b
# define        OSF_PRESENT_REL_FMT       0x4c
# define        OSF_READ_PARTITION        0x01
# define          OSF_RP_TYPE_QUERY         0x02 /* type fields for read partition sf */
# define          OSF_RP_TYPE_QUERY_LIST    0x03
# define          OSF_RP_TYPE_RMA           0x6e
# define          OSF_RP_TYPE_RB            0xf2
# define          OSF_RP_TYPE_RM            0xf6

# define        OSF_READ_PARTITION_Q_LST  0x0103
# define        OSF_READ_PARTITION_RMA    0x016e
# define        OSF_READ_PARTITION_RB     0x01f2
# define        OSF_READ_PARTITION_RM     0x01f6

# define        OSF_REQUEST_RECOV_DATA    0x1030
# define        OSF_RESET_PARTITION       0x00
# define        OSF_RESTART               0x1033
# define        OSF_SCS_DATA              0x41
# define        OSF_SELECT_COLOR_TABLE    0x0f04
# define        OSF_SELECT_FORMAT_GROUP   0x4a
# define        OSF_SET_CHKPT_INTVL       0x1032
# define        OSF_SET_MSR_CONTROL       0x0f01
# define        OSF_SET_PARTITION_CHR     0x0f08
# define        OSF_SET_PRINTER_CHR       0x0f84
# define        OSF_SET_REPLY_MODE        0x09
# define        OSF_SET_WINDOW_ORIGIN     0x0b

/* ID's for Outbound/Inbound Structured Fields
 * These are actually the values of byte 3 as byte 2 is
 * always '0x0f'
 */

# define        OISF_DATA_CHAIN           0x21
# define        OISF_DEST_ORIGIN          0x02
# define        OISF_OBJ_CONTROL          0x11
# define        OISF_OBJ_DATA             0x0f
# define        OISF_OBJ_PICTURE          0x10
# define        OISF_OEM_DATA             0x1f
# define        OISF_SELECT_IPDS          0x83

# define        OISF_SAVE_RESTORE_FMT     0x1034


/* Spanning flags for above commands */

# define        SPAN_MASK                 0xc0
# define        SPAN_MIDDLE               0x00
# define        SPAN_LAST                 0x40
# define        SPAN_FIRST                0x80
# define        SPAN_ONLY                 0xc0

/* Mode flags for above commands */

# define        MODE_DRAW                 0x00
# define        MODE_STORE                0x20
# define        MODE_STORE_AND_DRAW       0x30

/* Procedural Instructions; 3.3.3 GA18-2261-0 */

# define        GERASE                    0x0a /* erase_graphics_presentation_space */
# define        GSTOPDR                   0x0f /* stop_draw */
# define        GATTCUR                   0x08 /* attach_graphic_cursor */
# define        GDETCUR                   0x09 /* detach_graphic_cursor */
# define        GSETCUR                   0x31 /* set_graphic_cursor_position */
# define        GSCUDEF                   0x21 /* set_current_defaults */
# define        GNOP1                     0x00 /* proc_noop */
# define        GCOMNT                    0x01 /* proc_comment */

/* Drawing Orders; 3.3.2 GA18-2261-0 */

# define        GBAR			  0x68 /* begin_area */
# define	GBIMG			  0xd1 /* begin_image */
# define	GCBIMG			  0x91 /* begin_image_cp */
# define 	GCHST			  0xc3 /* character_string */
# define 	GCCHST			  0x83 /* character_string_cp */
# define 	GEAR			  0x60 /* end_area */
# define	GEIMG			  0x93 /* end_image */
# define	GFLT			  0xc5 /* fillet */
# define	GCFLT			  0x85 /* fillet_cp */
# define	GFARC			  0xc7 /* full_arc */
# define	GCFARC			  0x87 /* full_arc_cp */
# define	GIMD			  0x92 /* image_data */
# define	GLINE			  0xc1 /* line */
# define	GOLIN			  0x81 /* line_cp */
# define	GMRK			  0xc2 /* marker */
# define	GCMRK			  0x82 /* marker_cp */
# define	GRLINE			  0xe1 /* relative_line */
# define	GCRLIN			  0xa1 /* relative_line_cp */
# define	GSBMX			  0x0d /* set_background_mix */
# define	GSCA			  0x34 /* set_character_angle */
# define	GSCC			  0x33 /* set_character_cell */
# define	GSCD			  0x3a /* set_character_direction */
# define	GSCR			  0x39 /* set_character_precision */
# define	GSCS			  0x38 /* set_character_set */
# define	GSCH			  0x35 /* set_character_shear */
# define	GSCOL			  0x0a /* set_color */
# define	GSECOL			  0x26 /* set_extended_color */
# define	GSLT			  0x18 /* set_line_type */
# define	GSLW			  0x19 /* set_line_width */
# define	GSMC			  0x37 /* set_marker_cell */
# define	GSMP			  0x3b /* set_marker_precision */
# define	GSMS			  0x3c /* set_marker_set */
# define	GSMT			  0x29 /* set_marker_symbol */
# define	GSMX			  0x0c /* set_mix */
# define	GSPS			  0x08 /* set_pattern_set */
# define	GSPT			  0x28 /* set_pattern_symbol */
# define	GCOMT			  0x01 /* draw_comment */
# define	GNOP1			  0x00 /* draw_noop */
# define	GSAP			  0x22 /* set_arc_params */
# define	GSCP			  0x21 /* set_current_position */
# define	GSGCH			  0x04 /* segment_characteristics */
# define	GEPROL			  0x3e /* end_prologue */
# define	GESD			  0xff /* end_symbol_definition */

/* ID's for Inbound Structured Fields, Byte 2 */

# define        ISF_INBOUND_SF            0x80
# define        ISF_QUERY_REPLY           0x81

/* ID's for Inbound Structured Fields Query Reply, Byte 3 */

# define        ISF_QR_SUMMARY            0x80
# define        ISF_QR_UAREA              0x81 /* 129 decimal */
# define        ISF_QR_ALPHA_PART         0x84
# define        ISF_QR_CHARSETS           0x85 /* 133 decimal */
# define        ISF_QR_COLOR              0x86 /* 134 decimal */
# define        ISF_QR_HIGHLIGHT          0x87
# define        ISF_QR_REPLY_MODE         0x88
# define        ISF_QR_OEM_AUX_DEV        0x8f
# define        ISF_QR_STORAGE_POOLS      0x96
# define        ISF_QR_AUX_DEV            0x99
# define        ISF_QR_RPQNAMES           0xa1 /* ??????????? */ 
# define        ISF_QR_IMPLICIT_PART      0xa6 /* 166 decimal */
# define        ISF_QR_BACKGRND_TRANSP    0xa8
# define        ISF_QR_SEGMENT            0xb0
# define        ISF_QR_PROCEDURE          0xb1
# define        ISF_QR_LINE_TYPE          0xb2
# define        ISF_QR_PORT               0xb3 /* 179 decimal */
# define        ISF_QR_GRAPHIC_COLOR      0xb4
# define        ISF_QR_GRAPHIC_SYM_SET    0xb6 /* 182 decimal */
# define        ISF_QR_NULL               0xff

/* Fields of a Write Control Character (WCC) */

# define	WCC_RESET	0x40	/* Reset Bit */
# define	WCC_FORMAT	0x30	/* Printout Format */
#	define	WCC_132		0x00	/* 132-column lines */
#	define	WCC_40		0x10	/* 40-column lines */
#	define	WCC_64		0x20	/* 64-column lines */
#	define	WCC_80		0x30	/* 80-column lines */
# define	WCC_START	0x08	/* Start Printer */
# define	WCC_ALARM	0x04	/* Sound Alarm */
# define	WCC_RESTORE_KBD	0x02	/* Restore Keyboard */
# define	WCC_RMDT	0x01	/* Reset Modified Data Tags */
 
/* Fields of an Attribute Byte */
# define	ATTR_TOP	0xc0	/* top bits of the attribute */
# define	ATTR_PROT	0x20	/* protected field */
# define	ATTR_NUMERIC	0x10	/* numeric field */
# define	ATTR_DISPLAY	0x0c	/* display attributes */
#	define	ATTR_LN		0x00	/* low intensity / no pen detect */
#	define	ATTR_LP		0x04	/* low intensity / pen detect */
#	define	ATTR_HP		0x08	/* high intensity / pen detect */
#	define	ATTR_IN		0x0c	/* invisible / no pen detect */
# define	ATTR_MDT	0x01	/* Modified Data Tag */


/* types for Extanded Field Attribute */
# define        EFA_RESET       0x00
# define        EFA_FIELD       0xc0
# define        EFA_EXT_HIGH    0x41
# define        EFA_COLOR       0x42
# define        EFA_P_SYM       0x43
# define        EFA_BG_TRANSP   0x46


 
/* buffer address modes */
# define	FOURTEEN_BIT	0x00	/* 14-bit buffer address mode */
# define	TWELVE_BIT	0x40	/* 12-bit buffer address mode */
# define	ADDR_PART	0x3f	/* address part of the byte */
 
/* Attention Identifications */
 
# define	AID_NONE	0x60	/* No AID generated (display) */
# define	AID_ENTER	0x7d	/* ENTER key */
# define	AID_PF1		0xf1	/* PF1 key */
# define	AID_PF2		0xf2	/* PF2 key */
# define	AID_PF3		0xf3	/* PF3 key */
# define	AID_PF4		0xf4	/* PF4 key */
# define	AID_PF5		0xf5	/* PF5 key */
# define	AID_PF6		0xf6	/* PF6 key */
# define	AID_PF7		0xf7	/* PF7 key */
# define	AID_PF8		0xf8	/* PF8 key */
# define	AID_PF9		0xf9	/* PF9 key */
# define	AID_PF10	0x7a	/* PF10 key */
# define	AID_PF11	0x7b	/* PF11 key */
# define	AID_PF12	0x7c	/* PF12 key */
# define	AID_PF13	0xc1	/* PF13 key */
# define	AID_PF14	0xc2	/* PF14 key */
# define	AID_PF15	0xc3	/* PF15 key */
# define	AID_PF16	0xc4	/* PF16 key */
# define	AID_PF17	0xc5	/* PF17 key */
# define	AID_PF18	0xc6	/* PF18 key */
# define	AID_PF19	0xc7	/* PF19 key */
# define	AID_PF20	0xc8	/* PF20 key */
# define	AID_PF21	0xc9	/* PF21 key */
# define	AID_PF22	0x4a	/* PF22 key */
# define	AID_PF23	0x4b	/* PF23 key */
# define	AID_PF24	0x4c	/* PF24 key */
# define	AID_SELECT	0x7e	/* Selector light pen */
# define	AID_PA1		0x6c	/* PA1 key */
# define	AID_PA2		0x6e	/* PA2 (CNCL) key */
# define	AID_PA3		0x6b	/* PA3 key */
# define	AID_CLEAR	0x6d	/* CLEAR key */
# define	AID_SYSREQ	0xf0	/* SYS REQ key */


# define	AID_ISF	        0x88	/* Inbound SF reply */


# define        DEFAULT_LINES   24
# define        DEFAULT_COLUMNS 80
 
# define	SCREEN_SIZE		(43 * 132) /* worst case assumption */
# define	COMMAND_BUFFER_SIZE	(3 * SCREEN_SIZE)

/* SET CURRENT DEFAULT OSF */

# define  DEF_DRAW_ATTR	    0x00
# define  DEF_LINE_ATTR     0x01
# define  DEF_CHAR_ATTR     0x02
# define  DEF_MARKER_ATTR   0x03
# define  DEF_PATTERN_ATTR  0x04
# define  DEF_ARC_PARAM     0x0B
/* ******* */


extern Byte	command_buffer[];
extern Byte	*cbufp;
extern int      deferred_update;
extern int graphic_cursor ;
extern int xcurs,ycurs;
extern int  us_start_row, us_start_col;
extern int  us_stop_row, us_stop_col;
extern int  char_width, char_height ;
extern int  screen_width, screen_height ;

/* unsable area */
#define UROW_INDEX1   7
#define UROW_INDEX2  32
#define UCOL_INDEX1   5
#define UCOL_INDEX2  30

/* impl partition */
#define IROW_INDEX1  10
#define IROW_INDEX2  14
#define ICOL_INDEX1   8
#define ICOL_INDEX2  12

