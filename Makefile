# Generated automatically from Makefile.in by configure.
#
# $Id$
#

PROG	= null
OBJS	= argv.o compat.o md5.o

PORTFLS	= ChangeLog Makefile.all.in argv.[ch] argv_loc.h compat.[ch] \
	conf.h.in configure configure.in install-sh md5.[ch] md5_loc.h \
	mkinstalldirs null.c

DEFS	= -DHAVE_STDLIB_H=1 \
	-DHAVE_STRING_H=1 \
	-DHAVE_UNISTD_H=1 \
	$(DEFINES)
INCS	= -I. $(INCLUDES)
LIBS	= $(LIBRARIES)

CFLAGS	= $(CCFLAGS)
DESTDIR	= /usr/local/local/bin

all :: $(PROG)

clean ::
	$(CLEAN)
	rm -f $(PROG)

install :: $(PROG)
	install -cesv $(PROG) $(DESTDIR)

$(PROG) : $(PROG).o $(OBJS)
	rm -f $@
	$(CC) $(LDFLAGS) $(PROG).o $(OBJS) $(LIBS)
	mv a.out $@

port :: $(PORTFLS)
	@ answer 'Is it okay to remove the $@ directory? '
	rm -rf $@
	mkdir $@
	cp $(PORTFLS) $@
	mv $@/Makefile.all.in $@/Makefile.in
	@ echo ''
	@ echo 'Please rename $@ to argv-version and tar up file'

.include <local.mk>
