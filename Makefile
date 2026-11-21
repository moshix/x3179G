# Makefile - build x3179 on macOS (Apple Silicon or Intel) and on
# Linux (amd64, also any other 64-bit Linux).  The OS is detected with
# uname; override with "make UNAME_S=Linux" if needed.
#
#   make                  build ./x3179
#   make test             build + run tests/host_tests.sh, tests/smoke.sh
#   make test-local       build + run tests/host_tests.sh (no network)
#   make clean
#
# macOS: needs XQuartz (https://www.xquartz.org), which provides
# Xt/Xaw/Xmu under /opt/X11.  Homebrew's X11 packages lack libXaw, so
# we use XQuartz exclusively to avoid mixing two X11 installations.
#
# Linux: needs a C compiler, make, pkg-config and the X11 development
# packages.  Debian/Ubuntu:
#   apt install build-essential pkg-config libxaw7-dev libxmu-dev \
#               libxt-dev libxext-dev libx11-dev
# Fedora: dnf install gcc make pkgconf libXaw-devel libXmu-devel \
#               libXt-devel libXext-devel libX11-devel
#
# NOTE: macOS file systems are case-insensitive, so the binary "x3179"
# and the old app-defaults copy "X3179" are the SAME file.  Only
# X3179.ad is kept; it is compiled into the binary (see x3179_ad.h).

UNAME_S ?= $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
X11         ?= /opt/X11
X11_CFLAGS   = -I$(X11)/include
# rpath so the binary finds XQuartz's libraries without DYLD_* tricks
X11_LDFLAGS  = -L$(X11)/lib -Wl,-rpath,$(X11)/lib
else ifeq ($(UNAME_S),Linux)
# X11 headers and libraries live in the system paths; pkg-config adds
# anything a distribution puts elsewhere.  Errors are hidden so a
# missing pkg-config just means "use the system paths".
X11_PKGS     = xaw7 xmu xt xext x11
X11_CFLAGS  := $(shell pkg-config --cflags $(X11_PKGS) 2>/dev/null)
X11_LDFLAGS := $(shell pkg-config --libs-only-L $(X11_PKGS) 2>/dev/null)
else
$(error unsupported OS "$(UNAME_S)": only Darwin and Linux are known)
endif

# This is 1991-93 K&R C.  gnu89 keeps implicit int / K&R definitions
# legal; the -Wno-error flags undo errors that modern compilers (clang,
# gcc 14+) turned on by default for constructs this code relies on.
#
# Not every compiler knows every flag (incompatible-function-pointer-
# types is clang-only, return-mismatch is gcc 14+), and an unknown one
# can itself be an error.  So each flag is tried once on an empty file
# with -Werror, and only the ones the compiler accepts are used.
WNOERROR = implicit-function-declaration implicit-int int-conversion \
           incompatible-pointer-types incompatible-function-pointer-types \
           return-type return-mismatch declaration-missing-parameter-type
cc_accepts = $(shell $(CC) -Werror $(1) -x c -c /dev/null -o /dev/null \
                     >/dev/null 2>&1 && echo $(1))
CSTD     = -std=gnu89
CWARN   := -w $(foreach w,$(WNOERROR),$(call cc_accepts,-Wno-error=$(w)))
CDEFS    = -DHAVE_ASYNCIO
CFLAGS  ?= -O
ALL_CFLAGS = $(CFLAGS) $(CSTD) $(CWARN) $(CDEFS) -I. $(X11_CFLAGS)

LDFLAGS += $(X11_LDFLAGS)
LDLIBS   = -lXaw -lXmu -lXt -lSM -lICE -lXext -lX11

# strcasecmp.c is omitted: libc already provides strcasecmp/strncasecmp
# and the local K&R copy conflicts with the system prototypes.
SRCS = x3179.c ColorScreen.c eventhdlr.c netio.c graphics.c \
       fs3270.c asc_ebc.c tables.c
OBJS = $(SRCS:.c=.o)

x3179: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

# X3179.ad is compiled into x3179 as default resources, so the colours
# and key bindings work without XENVIRONMENT.  Each line becomes a C
# string ending in \n: backslashes and quotes are escaped first, so a
# trailing "\" continuation line survives as backslash-newline.
x3179_ad.h: X3179.ad
	{ echo '/* generated from X3179.ad by Makefile */'; \
	  echo 'static char builtin_resources[] ='; \
	  sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' \
	      -e 's/^/"/' -e 's/$$/\\n"/' X3179.ad; \
	  echo ';'; } > $@

x3179.o: x3179_ad.h

# Rebuild everything when any header changes: several files share the
# ColorScreen widget struct, and stale objects would use old offsets.
$(OBJS): $(wildcard *.h)

test: test-local
	sh tests/smoke.sh

test-local: x3179
	sh tests/host_tests.sh

clean:
	rm -f $(OBJS) x3179 x3179_ad.h

.PHONY: test test-local clean
