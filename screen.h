typedef struct {
  short offset; /* offset from attribute byte */
  Byte  ch;     /* EBCDIC character at this screen location */
  ulong attr;   /* extended attributes for the current field */
} ScreenChar;

