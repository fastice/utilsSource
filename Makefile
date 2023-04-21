#
# Modify this section to point to where stuff is.
# Current names are for a specific file system.
# ROOTDIR: root directory for code (e.g. /Users/username directory). Likely should change for linux
# PROGDIR: location for top source code directory (default ROOTDIR/progs/GIT64)
# BINHOME: root directory for binaries
# BINNAME: archetecture dependent basename for bin dir
# BINDIR: directory for binaries (default BINHOME/bin/BINNAME) (will create if doesn't exist)
# INCLUDEPATH: include path (default PROGDIR anything else could cause a problem)
# Various directors can be overridden with environment variable or from make command
# make BINHOME=/a/path/to/binhome
#
# Base directory for code
USER =	$(shell id -u -n)
#
# Default rootdir
ifneq ($(ROOTDIR)),)
	ROOTDIR =	/Users/$(USER)
endif
$(info ROOTDIR="$(ROOTDIR)")
# Default root for source code
ifneq ($(PROGDIR)),)
	PROGDIR =       $(ROOTDIR)/progs/GIT64
endif
$(info PROGDIR ="$(PROGDIR)")
#
# Default location root for compiled programs
ifneq ($(BINHOME)),)
	BINHOME =		~$(USER)
endif
$(info BINHOME="$(BINHOME)")
#
# For historical reasons, can compile with 32-bit memory model using MEM=-m32
# In almost all cases, should be compiled as 64bit.
ifneq ($(MEM),-m32)
	BINNAME=	$(MACHTYPE)
	FFTDIR = $(MACHTYPE)-$(OSTYPE)
else
	BINNAME =	i386
	FFTDIR = i386-$(OSTYPE)
endif
$(info BINNAME="$(BINNAME)")
#
# Default binary directory
ifneq ($(BINDIR)),)
	BINDIR =	$(BINHOME)/bin/$(BINNAME)
endif
$(info BINDIR="$(BINDIR)")
#
# Create bin dir if it doesn't exist
$(shell mkdir -p $(BINDIR))
#
#
# Default include path
ifneq ($(INCLUDEPATH)),)
	INCLUDEPATH =	$(PROGDIR)
endif
$(info INCLUDEPATH ="$(INCLUDEPATH)")
#
# Compiler stuff
#
C =		gcc
CFLAGS =	'-O3 $(MEM) -I$(INCLUDEPATH) $(COMPILEFLAGS)'
CCFLAGS =  '-O3 $(MEM) $(COMPILEFLAGS)'
#-Wunused-variable'

CCFLAGS1= '-O3'
# uncomment to debug
#CFLAGS =	'-g $(MEM) -I$(INCLUDEPATH) $(COMPILEFLAGS)'
#CCFLAGS =  '-g $(MEM) -D$(MACHTYPE) $(COMPILEFLAGS)'
#CCFLAGS1= '-g'
#
# ******** SHOULD NOT NEED TO MODIFY BELOW HERE *********
#

RECIPES  =	$(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/polint.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/nrutil.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/ratint.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/four1.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svdfit.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svdcmp.o \
		$(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svdvar.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svbksb.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/pythag.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/hunt.o

STANDARD =	$(PROGDIR)/clib/$(MACHTYPE)-$(OSTYPE)/standard.o

UNWRAP = $(PROGDIR)/unwrapSource/unWrap/$(MACHTYPE)-$(OSTYPE)/labelRegions.o

CULLST = $(PROGDIR)/speckleSource/Cullst/$(MACHTYPE)-$(OSTYPE)/cullIslands.o

TARGETS = intfloat changeflat

# $(PROGDIR)/speckleSource
all: $(TARGETS)

INTFLOAT=	intFloat/$(MACHTYPE)-$(OSTYPE)/intfloat.o intFloat/$(MACHTYPE)-$(OSTYPE)/intFloatImage.o intFloat/$(MACHTYPE)-$(OSTYPE)/getPhaseImage.o
INTFLOATDIRS =	 intFloat  $(PROGDIR)/cRecipes $(PROGDIR)/unwrapSource/unWrap $(PROGDIR)/clib $(PROGDIR)/speckleSource/Cullst

intfloat:	
	@for i in ${INTFLOATDIRS}; do \
		( 	echo "<<< Descending in directory: $$i >>>"; \
	                cd $$i; \
			make FLAGS=$(CCFLAGS) INCLUDEPATH=$(INCLUDEPATH) PAF=0; \
			cd $(PROGDIR); \
		); done
		gcc $(MEM) $(CCFLAGS1) \
                $(INTFLOAT)  $(STANDARD) $(RECIPES) $(UNWRAP)  $(CULLST) \
                -lm  -o $(BINDIR)/intfloat


CHANGEFLAT =	changeFlat/$(MACHTYPE)-$(OSTYPE)/changeflat.o changeFlat/$(MACHTYPE)-$(OSTYPE)/reflattenImage.o changeFlat/$(MACHTYPE)-$(OSTYPE)/inputParamFile.o 
CHANGEFLATDIRS =	changeFlat

changeflat :	
	@for i in ${CHANGEFLATDIRS}; do \
		( 	echo "<<< Descending in directory: $$i >>>"; \
	                cd $$i; \
			make FLAGS=$(CCFLAGS) INCLUDEPATH=$(INCLUDEPATH) PAF=0; \
			cd $(PROGDIR); \
		); done
		gcc $(MEM) $(CCFLAGS1) \
                $(CHANGEFLAT)  $(STANDARD) $(RECIPES) $(UNWRAP)  $(CULLST) intFloat/$(MACHTYPE)-$(OSTYPE)/getPhaseImage.o \
                -lm  -o $(BINDIR)/changeflat


