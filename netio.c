/*
 *	Network I/O
 *	Telnet option negotiation
 *
 * 	all routines called from the frontend return 0 
 *	on success or -1 on failure. They also store an 
 *	appropriate err mesage in error_message.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>	/* exit */
#include <string.h>	/* strcpy, memcpy (bcopy) */
#include <unistd.h>	/* read, write, close */
#include <arpa/inet.h>	/* htons */

#include <assert.h>
#include <X11/Intrinsic.h>	

#include "global.h"
#include "x3270.h"
#include "dbug.h"
#define TELOPTS
#include "telnet.h"
#include "ibm3270.h"

/* "#define XtPointer int" removed: it truncates pointers on 64-bit */
#define	IS	'\000'
#define	SEND	'\001'
#define	strip(x)	((x)&0177)
#define XCTRL(c) ('c' & 037)

#define MAX_NETWORK_WRITE_LENGTH 	2048
#define NET_BUFSIZ 			8192	/* mac compat */

Byte    command_buffer[COMMAND_BUFFER_SIZE];
Byte    *cbufp = command_buffer;
char	netobuf[NET_BUFSIZ], *nfrontp = netobuf, *nbackp = netobuf;
static	char	sibuf[NET_BUFSIZ], *sbp;
static	int	scc, tcc;


/*
 * 	option negotiation
 */
static	char	hisopts[256];
static	char	myopts[256];
static	char	doopt[] = { IAC, DO, '%', 'c', 0 };
static	char	dont[] = { IAC, DONT, '%', 'c', 0 };
static	char	will[] = { IAC, WILL, '%', 'c', 0 };
static	char	wont[] = { IAC, WONT, '%', 'c', 0 };
static	char	sb[] = { IAC, SB, TELOPT_TERMTYPE, IS };
static	char	subterm[] = { '%', 's', IAC, SE, 0 };
int	showoptions = 1;
int	options;
int	crmod = 0;
int	wordmode = 0;
int	transcript = -1;
char	*prompt;
char	*ibm_term = NULL;
char	*real_term = NULL;


/*
 *	socket I/O
 */
static	int	net_socket;
static 	char   	*offical_name;
static 	int 	non_blocking = FALSE; 		/* true during connect() */
static 	int 	output_callback_pending = 0;	/* see comments in net_write */
static  struct	sockaddr_in sin;

typedef enum { 
  disconnected, in_progress, connected 
  } ConnState;

ConnState	conn_state = disconnected;

/*
 *	Xt event sources
 */
static 	XtInputId input_id;
static 	XtInputId output_id;
static 	XtInputId except_id;

/* 	
 *	Xt event handler 
 */
static 	void 	net_readable();
static 	void 	net_write();
static 	void 	net_exception();

extern	int errno; 

/*
 *    	connect_to_host
 *
 *	- get offical hostname  
 *	- get port number if port==0
 *	- create socket
 *	- set to nonblocking during connect() so the Xt Main loop 
 *	  can continue during connect(). Once the socket is readable
 *	  (= connected) we switch back to blocking I/O.
 *	- setup Xt Input and Exception handler
 */
int
connect_to_host(hostname,port)
     char *hostname;
     int port;
{
  register struct hostent *host;
  struct	servent *sp;


  if (conn_state == connected) 
    {
      sprintf( error_message, "Already connected to %s", offical_name);
      return (-1);
    }
  
  if (conn_state == in_progress) 
    {
      sprintf( error_message, "Connectionin progress to %s", offical_name);
      return (-1);
    }
  
  if (!port)
    {
      sp = getservbyname("telnet", "tcp");
      if (sp == 0)
	{
	  strcpy( error_message,"tcp/telnet: unknown service");
	  return(-1);
	}    
      port = sp->s_port;		/* already network byte order */
    }
  else
    {
      /*
       * A port given via the *port resource is in host byte order.
       * The original code stored it into sin_port unconverted, which
       * only worked on big-endian hosts (SPARC, RS/6000).
       */
      port = htons(port);
    }

  host = gethostbyname(hostname);
  if (host) 
    {
      sin.sin_family = host->h_addrtype;
      bcopy(host->h_addr, (caddr_t)&sin.sin_addr, host->h_length);
      offical_name = hostname = host->h_name;
    } 
  else 
    {
      offical_name = "*unknown*";
      sprintf(error_message, "%s: unknown host", hostname);
      return (-1);
    }
  sin.sin_port = port;
  
  net_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (net_socket < 0) 
    {
      sprintf(error_message, "error during socket(): errno=%d", errno);
      return(-1);
    }
#ifdef HAVE_ASYNCIO  
  fcntl(net_socket, F_SETFL,FNDELAY);		/* sysdep */

  non_blocking = TRUE;
#else
  non_blocking = FALSE;
#endif


#ifdef HAVE_ASYNCIO    
  /*
   * Non-blocking connect: -1/EINPROGRESS is the normal result.
   * Only inspect errno when connect() actually failed; the old code
   * tested errno unconditionally and could trip on a stale value.
   */
   if (connect(net_socket, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
  if (errno != EINPROGRESS) 
    {
      sprintf(error_message, "error during connect(): errno=%d", errno);
      return(-1);
    } }

  conn_state = in_progress;
#else

  if (connect(net_socket, (struct sockaddr *)&sin, sizeof (sin)) == -1)
    {
      sprintf(error_message, "error during connect(): errno=%d", errno);
      return(-1);
    }

  conn_state = connected;
#endif

  input_id  = XtAppAddInput( app_con, net_socket, 
			    (XtPointer) XtInputReadMask,
			    net_readable, NULL);

  except_id = XtAppAddInput( app_con, net_socket,
			    (XtPointer) XtInputExceptMask,
			    net_exception, NULL);

  return 0;
}

extern update_screen();
/*
 *	this is called from XtAppMainLoop if the socket
 *	becomes readable.
 */
static void 
net_readable(client_data, source, id)
     XtPointer client_data;
     int *source;
     XtInputId *id;
{
   extern Widget screen_widget;
   static int icont = 0;
  DBUG_ENTER("net_socket_readable");
  
  DBUG_PRINT("net_socket_readable", ("socket fd=%d\n", *source));
  
  deferred_update = TRUE;
  if (non_blocking) 
    {
      int l;
      l = fcntl(*source, F_GETFL,0);		/* sysdep */
      l &= ~FNDELAY;
      fcntl(*source, F_SETFL,l);
      conn_state = connected;
      DBUG_PRINT("net_socket_readable", ("switch to blocking\n"));
      non_blocking = 0;
    }
  
  if ((scc = read(*source,sibuf, sizeof(sibuf))) < 0) 
    {
      sprintf(error_message, "error during read(): errno=%d", errno);
      perror("read");
      scc = 0;
    } 
  else if (scc == 0) 
    {
      DBUG_PRINT("net_read",("net_input: read 0 bytes\n"));
      exit(0);
    }
  else
    {
      sbp = sibuf;
      DBUG_PRINT("net_read",("net_socket_readable: got %d: \n", scc));
      DBUG_HEX("net",sibuf,scc);
      telrcv();
    }
  net_flush();
  deferred_update = FALSE;

  update_screen();
  DBUG_VOID_RETURN;
}


/*
 *	probably called on remote close ?
 */

static void
net_exception(client_data, source, id)
     XtPointer client_data;
     int *source;
     XtInputId *id;
{
  DBUG_ENTER("net_socket_exception");
  DBUG_PRINT("net_socket_exception", ("socket fd=%d\n", *source));
  printf("NET EXCEPTION -probably closed\n");

  DBUG_VOID_RETURN;
}

/*
 *	Shutdown the socket, and remove all Xt event handlers.
 */
int
disconnect_from_host()
{
  if (conn_state != disconnected) 
    {
      XtRemoveInput(input_id);
      if (output_callback_pending)
	XtRemoveInput(output_id);
      XtRemoveInput(except_id);

      nbackp = nfrontp = netobuf;

      shutdown(net_socket,2);			/* sysdep */
						/* wont you believe i:t */
						/* ix386's shutdown wont work */
      conn_state = disconnected;

      return 0;
    } 
  else 
    {
      strcpy(error_message,"not connected");
      return (-1);
    }
}

/*
 *	this is called from both net_flush() and the
 *	XtAppMainLoop. The callers are distinguished 
 *	by the client_data argument.
 */
static void 
net_write(called_from_callback, source, id)
     XtPointer called_from_callback;
     int *source;		/* socket */
     XtInputId *id;		/* unused  */
{
  int n;
  
  DBUG_ENTER("net_write");
  
  /*
   *	if netflush() is called while a callback is pending,
   *	ignore it. It will all be flushed by the callbacks.
   */
  if (!called_from_callback && output_callback_pending)
    return;
  
  /* 
   *	something to write at all?
   */
  if ((n = nfrontp - nbackp) > 0) 
    { 
      
      if (n > MAX_NETWORK_WRITE_LENGTH) 
	{	
	  /* 
	   *	If there's more data queued than we can handle in 
	   *	a single write, we will setup a XtAddInput calback
	   *	so we get notified once the socket is drained.
	   */
	  n = MAX_NETWORK_WRITE_LENGTH;
	  
	  if (!output_callback_pending)
	    {
	      /*
	       * have Toolkit call us back once the
	       * socket is again writable.
	       */
	      DBUG_PRINT("net_write",("adding input"));
	      output_id = XtAppAddInput( app_con, *source,
					(XtPointer) XtInputWriteMask,
					net_write, (XtPointer) TRUE);
	      output_callback_pending = TRUE;
	    }
	  
	} 
      else 
	{
	  /*
	   *	Everything can be handled in a single write. 
	   *	If we had a callback pending, remove it now.
	   */
	  if (output_callback_pending)
	    {
	      DBUG_PRINT("net_write",("removing input"));
	      XtRemoveInput(output_id);
	      output_callback_pending = FALSE;
	    }
	}
      DBUG_PRINT("net",("write: socket is %d\n", *source));
      n = write(*source, nbackp, n);
      DBUG_PRINT("net",
		 ("write: have %d, did %d chars, pending=%d, .here's what I sent:...:\n", 
		  nfrontp-nbackp,n,output_callback_pending));
      
      DBUG_HEX("net",nbackp,n);
      
      if (n < 0) {
	if (errno != ENOBUFS && errno != EWOULDBLOCK) {
	  sprintf(error_message, 		  "error during write(): errno=%d", errno);	  
	  perror(offical_name);
	  close(*source);	/* give up */
	}
	n = 0;
      }
      nbackp += n;
      if (nbackp == nfrontp) {
	nbackp = nfrontp = netobuf;
      }
    }	/* end of 'something to write  */
  DBUG_VOID_RETURN;
}

/* 
 *	get things going. This is called  from any point
 *	in the program where a complete message has been
 *	assembled in netobuf[].
 */
net_flush()
{
  int n;
  
  DBUG_ENTER("net_flush");
  DBUG_PRINT("net",("fd = %d\n",net_socket));
  
  net_write( (XtPointer) FALSE, &net_socket, NULL);
  
  DBUG_VOID_RETURN;
}


/*
 * Telnet receiver states for fsm
 */
#define	TS_DATA		0
#define	TS_IAC		1
#define	TS_WILL		2
#define	TS_WONT		3
#define	TS_DO		4
#define	TS_DONT		5
#define	TS_SB		6
#define	TS_TERMTYPE	7


telrcv()
{
  register int c;
  static int state = TS_DATA;
  static int sent_real_term = 0;
  
  DBUG_ENTER("telrcv");
  
  while (scc > 0) {
    c = *sbp++ & 0377, scc--;
    switch (state) {
      
    case TS_DATA:
      if (c == IAC) {
	state = TS_IAC;
	continue;
      }
      /* add to command buffer */
      *cbufp++ = c;
      continue;
      
    case TS_IAC:
      switch (c) {
	/* 
	 *	a double IAC is stored as 
	 *	a single IAC -mah 
	 */
      case IAC: 
	
	*cbufp++ = IAC;
	state = TS_DATA;
	break;
	
      case WILL:
	state = TS_WILL;
	continue;
	
      case WONT:
	state = TS_WONT;
	continue;
	
      case DO:
	state = TS_DO;
	continue;
	
      case DONT:
	state = TS_DONT;
	continue;
	
      case DM:
      case NOP:
      case GA:
	break;
	
      case SB:
	state = TS_SB;
	continue;
	
      case SE:
	break;
	
      case EOR:
	fs_decode (command_buffer, 
		   cbufp-command_buffer);
	cbufp = command_buffer;			/* reset bp */
	break;
	
      default:
	break;
      }
      state = TS_DATA;
      continue;
      
    case TS_WILL:
      printoption("RCVD", will, c, !hisopts[c]);
      if (!hisopts[c])
	willoption(c);
      state = TS_DATA;
      continue;
      
    case TS_WONT:
      printoption("RCVD", wont, c, hisopts[c]);
      if (hisopts[c])
	wontoption(c);
      state = TS_DATA;
      continue;
      
    case TS_DO:
      printoption("RCVD", doopt, c, !myopts[c]);
      if (!myopts[c])
	dooption(c);
      state = TS_DATA;
      continue;
      
    case TS_DONT:
      printoption("RCVD", dont, c, myopts[c]);
      if (myopts[c]) {
	myopts[c] = 0;
	sprintf(nfrontp, wont, c);
	nfrontp += sizeof (wont) - 2;
	printoption("SENT", wont, c, 0);
      }
      state = TS_DATA;
      continue;
      
    case TS_SB:
      printoption("RCVD", sb, c, 0);
      if (c == TELOPT_TERMTYPE)
	state = TS_TERMTYPE;
      continue;
      
    case TS_TERMTYPE:
      if (c == SEND) {
	printoption("RCVD", subterm, c, 1);
	bcopy (sb, nfrontp, sizeof sb);
	nfrontp += sizeof sb;
	sprintf(nfrontp, subterm, ibm_term);
	nfrontp += strlen (ibm_term) + 2;
	printoption("SENT", subterm, c, 0);
      }
      else
	printoption("RCVD", subterm, c, 0);
      state = TS_DATA;
      continue;
    }
  }
  DBUG_VOID_RETURN;
}

willoption(option)
     int option;
{
  char *fmt;
  
  switch (option) {
    

  case TELOPT_EOR:
    hisopts[option] = 1;
    fmt = doopt;
    break;

  case TELOPT_ECHO:
  case TELOPT_SGA:
    hisopts[option] = 1;
    fmt = doopt;
    break;
    
  case TELOPT_TM:
    fmt = dont;
    break;
    
  case TELOPT_BINARY:
    if (myopts[TELOPT_TERMTYPE]) {
      hisopts[option] = 1;
      cbufp = command_buffer;
      fs_init ();
    }
    fmt = doopt;
    break;
    
  default:
    fmt = dont;
    break;
  }
  sprintf(nfrontp, fmt, option);
  nfrontp += sizeof (dont) - 2;
  printoption("SENT", fmt, option, 0);
}

wontoption(option)
     int option;
{
  char *fmt;
  
  switch (option) {
    
  case TELOPT_ECHO:
  case TELOPT_SGA:
    hisopts[option] = 0;
    fmt = dont;
    break;
    
  default:
    fmt = dont;
  }
  sprintf(nfrontp, fmt, option);
  nfrontp += sizeof (doopt) - 2;
  printoption("SENT", fmt, option, 0);
}

dooption(option)
     int option;
{
  char *fmt;
  
  switch (option) {
    
  case TELOPT_EOR:
    fmt = will;
    myopts[option] = 1;
    break;
    
  case TELOPT_TM:
    fmt = wont;
    break;
    
  case TELOPT_ECHO:
    fmt = will;
    hisopts[option] = 0;
    break;
    
  case TELOPT_SGA:
    fmt = will;
    break;
    
  case TELOPT_BINARY:
    fmt = will;
    myopts[option] = 1;
    break;
    
  case TELOPT_TERMTYPE:
    if (fs_select_term ()) {
      fmt = will;
      myopts[option] = 1;
      break;
    }
    fputs ("Terminal type not set - can not negotiate.\n",
	   stderr);
    /* fall through... */
    
  default:
    fmt = wont;
    break;
  }
  sprintf(nfrontp, fmt, option);
  nfrontp += sizeof (doopt) - 2;
  printoption("SENT", fmt, option, 0);
}


/*VARARGS*/
printoption(direction, fmt, option, what)
     char *direction, *fmt;
     int option, what;
{
  DBUG_ENTER("printoptions");
  
  if (!showoptions) {
    DBUG_VOID_RETURN;
  }
  
  DBUG_PRINT("popt", ("%s", direction));
  /* printf("%s ", direction);*/
  if (fmt == doopt)
    fmt = "do";
  else if (fmt == dont)
    fmt = "dont";
  else if (fmt == will)
    fmt = "will";
  else if (fmt == wont)
    fmt = "wont";
  else if (fmt == sb)
    fmt = "sb";
  else if (fmt == subterm)
    fmt = "(termtype send)";
  else
    fmt = "???";
  if (option <= TELOPT_EOR)
    DBUG_PRINT("popt",("%s %s", fmt, telopts[option]));
  else
    DBUG_PRINT("popt",("%s %d", fmt, option));
  if (*direction == '<') {
    DBUG_PRINT("popt",("\r\n"));
    DBUG_VOID_RETURN;
  }
  DBUG_PRINT("popt",(" (%s)\r\n", what ? "reply" : "don't reply"));
  DBUG_VOID_RETURN;
}


/*
 * add a buffer to the netout queue, escaping IAC's,
 * returns the actual number of bytes added 
 */
net_add_buf(buf,len)
     register Byte *buf;
     register int len;
{
    register actual = 0;
    
    assert((nfrontp + len - netobuf) < NET_BUFSIZ); /* optimistic */
    while (len--) {
	if (*buf == IAC) {
	    *nfrontp++ = IAC;
	    actual++;
	}
	*nfrontp++ = *buf++;
	actual++;
    }
    return actual;
}

/* add a single character, escaping if necessary 
 * return number of chars added
 */
net_add_ch(ch)
     register Byte ch;
{
    assert((nfrontp - netobuf) < NET_BUFSIZ); 
    *nfrontp++ = ch;
    if (ch == IAC) {
	*nfrontp++ = ch;
	return 2;
    } else return 1;
}

/*
 * add IAC-EOR sequence to netout q
 */
net_add_eor()
{
    assert((nfrontp - netobuf) < (NET_BUFSIZ - 1)); 
    *nfrontp++ = IAC;
    *nfrontp++ = EOR;
    return 2;
}


