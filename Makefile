#
# $Id$
#

INCS	= -I. -I/usr/local/inc $(INCLUDES)
LIBS	= -L/usr/local/lib -largv $(LIBRARIES)

CFLAGS	= $(CCFLAGS)
DESTDIR	= /usr/local/local/sbin
PROG	= null

all :: $(PROG)

clean ::
	$(CLEAN)
	rm -f $(PROG)

install :: $(PROG)
	install -cevR $(PROG) $(DESTDIR)

$(PROG) : $(PROG).o
	rm -f $@
	$(CC) $(LDFLAGS) $(PROG).o $(LIBS)
	mv a.out $@

.include <local.mk>
