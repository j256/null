#
# $Id$
#

PROG	= null

INCS	= -I. -I/usr/local/inc $(INCLUDES)
LIBS	= -L/usr/local/lib -largv $(LIBRARIES)

CFLAGS	= $(CCFLAGS)
DESTDIR	= /usr/local/local/bin

all :: $(PROG)

clean ::
	$(CLEAN)
	rm -f $(PROG)

install :: $(PROG)
	install -cesv $(PROG) $(DESTDIR)

$(PROG) : $(PROG).o
	rm -f $@
	$(CC) $(LDFLAGS) $(PROG).o $(LIBS)
	mv a.out $@

.include <local.mk>
