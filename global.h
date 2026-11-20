#ifndef global_h

#define global_h

#define DEFAULT_COLOR 0
#define BLUE  1
#define RED   2
#define PINK  3
#define GREEN 4
#define TURQUOISE 5
#define YELLOW 6
#define WHITE 7
#define BLACK 8

typedef unsigned char Byte;
typedef unsigned long Attr;


#ifdef SYSV
#define bcopy(a,b,n) memcpy(b,a,n)
#endif 

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif


/*
 * Character attributes are stored as 32bit values.
 *  
 * Interpretation of the lower 16bits:
 * +-----------+-----------+-----------+-----------+
 * |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 * +-----------+-----------+-----------+-----------+
 *  A  G  P  N  H  L   r  M  I  U  R  B <  color   >           
 *  T  R  R  U  I  O   e  D  N  N  E  L                   
 *  T  X  O  M  G  W   s  T  T  D  V  I                 
 *  c  -  T     H      d     R  E  E  N
 *  h  e                     A  R  R  K
 *  a  s  <regular attr>     N  L  S
 *  r  c                     S  I  E
 *                           P  N                 
 *                              E
 * Interpretation of the high 16bits:
 * +-----------------------+-----------------------+
 * |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 * +-----------------------+-----------------------+
 *                          <   character set id   >
 *  
 * <   currently unused    >
 *
 */
/* extend attribute byte interpretation in struct screenchar */

# define        SATTR_COLORMASK   0x0000000f    /* low 4 bits are colors */
# define        SATTR_C_DEFAULT   0x00000000    /* default: green */
# define        SATTR_C_BLUE      0x00000001  
# define        SATTR_C_RED       0x00000002  
# define        SATTR_C_PINK      0x00000003  
# define        SATTR_C_GREEN     0x00000004  
# define        SATTR_C_TURQUOISE 0x00000005  
# define        SATTR_C_YELLOW    0x00000006  
# define        SATTR_C_WHITE     0x00000007  

# define        SATTR_DISPLAYMASK 0x000000f0    /* default/blink/inverse/underline */
# define        SATTR_D_DEFAULT   0x00000000 
# define        SATTR_D_BLINK     0x00000010 
# define        SATTR_D_REVERSE   0x00000020 
# define        SATTR_D_UNDERLINE 0x00000040 

# define        SATTR_T_INTRANS   0x00000080    /* background intransparent  */
				                /* that is, text shadows graphics */

# define        SATTR_CS_MASK     0x00ff0000
# define        SATTR_CS_DEFAULT  0x00000000
# define        SATTR_CS_APL      0x00f10000    /* APL charset */

/* normal attribute byte interpretation in struct screenchar */

# define	SATTR_ATTRIB	0x00008000      /* Attribute byte at this location */
# define	SATTR_GRXESC	0x00004000	/* Graphics escape */
# define	SATTR_TOP	0x0000c000   
# define        SATTR_MASK      0x00003f00      /* old style attribute mask */
# define	SATTR_PROT	0x00002000	/* protected field */
# define	SATTR_NUMERIC	0x00001000	/* numeric field */
# define	SATTR_DISPLAY	0x00000c00	/* display attributes */
#	define	SATTR_LN	0x00000000	/* low intensity / no pen detect */
#	define	SATTR_IN	0x00000c00	/* invisible / no pen detect */
#	define	SATTR_HP	0x00000800	/* high intensity / pen detect */
#	define	SATTR_LP	0x00000400	/* low intensity / pen detect */
# define	SATTR_RESERVED  0x00000200      /* reserved */
# define	SATTR_MDT	0x00000100      /* Modified Data Tag */
# define        SATTR_LOW_UNP   0x00000060      /* unprotected low intensity */

/* convert 2 bytes to int; just to avoid alignment funnies */


/*#define FIELDLEN(bp) (short)((*((bp) + 1) & 0xff) | (*(bp) << 8))    */
#define FIELDLEN(bp)        ((*((bp) + 1) & 0xff) | (*(bp) << 8))



#endif
