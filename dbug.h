#    define DBUG_ENTER(a1)
#    define DBUG_RETURN(a1) return(a1)
#    define DBUG_VOID_RETURN return
#    define DBUG_EXECUTE(keyword,a1)
#    define DBUG_PRINT(keyword,arglist)
#    define DBUG_HEX(keyword, buf,len) /* mah 5/90 */
#    define DBUG_2(keyword,format)		/* Obsolete */
#    define DBUG_3(keyword,format,a1)		/* Obsolete */
#    define DBUG_4(keyword,format,a1,a2)	/* Obsolete */
#    define DBUG_5(keyword,format,a1,a2,a3)	/* Obsolete */
#    define DBUG_PUSH(a1)
#    define DBUG_POP()
#    define DBUG_PROCESS(a1)
#    define DBUG_FILE (stderr)
#    define DBUG_SETJMP setjmp
#    define DBUG_LONGJMP longjmp




