#
# All of the sources are in the src/ subdirectory so this just delegates to the src/Makefile
#

all :
	@ echo "Type 'make configure' to run configure down in the src/ subdirectory."
	@ echo "Type 'make build' to compile null down in the src/ subdirectory."
	@ echo "Type 'make test' to run the tests down in the src/ subdirectory."
	@ echo "Type 'make clean' to clean the compiled stuff."

build : src/Makefile
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
