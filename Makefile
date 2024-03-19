#
# All of the sources are in the src/ subdirectory so this just delegates to the src/Makefile
#

all : src/Makefile
	cd src ; make

src/Makefile :
	cd src ; ./configure

configure :
	cd src ; ./configure

clean :
	cd src ; make $@

distclean :
	cd src ; make $@

installdirs :
	cd src ; make $@

install :
	cd src ; make $@

test :
	cd src ; make $@
