C =		gcc
ROOTDIR =	/Users/ian
PROGDIR =       $(ROOTDIR)/progs/GIT
INCLUDEPATH =	$(ROOTDIR)/progs/GIT
BINDIR =	$(IHOME)/bin/$(MACHTYPE)
#
CFLAGS =	'-O3 -m32 -I$(INCLUDEPATH) $(COMPILEFLAGS)'
CCFLAGS =  '-O3 -m32 -D$(MACHTYPE) $(COMPILEFLAGS) '
#-Wunused-variable'

CCFLAGS1= '-O3'
# uncomment to debug
#CFLAGS =	'-g -m32 -I$(INCLUDEPATH) $(COMPILEFLAGS)'
#CCFLAGS =  '-g -m32 -D$(MACHTYPE) $(COMPILEFLAGS)'
#CCFLAGS1= '-g' 


RECIPES  =	$(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/polint.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/nrutil.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/ratint.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/four1.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svdfit.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svdcmp.o \
		$(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svdvar.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/svbksb.o $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/pythag.o \
                $(PROGDIR)/cRecipes/$(MACHTYPE)-$(OSTYPE)/hunt.o

STANDARD =	$(PROGDIR)/clib/$(MACHTYPE)-$(OSTYPE)/standard.o

UNWRAP = $(PROGDIR)/unwrapSource/unWrap/$(MACHTYPE)-$(OSTYPE)/labelRegions.o

CULLST = $(PROGDIR)/speckleSource/Cullst/$(MACHTYPE)-$(OSTYPE)/cullIslands.o

TARGETS = intfloat


all: $(TARGETS)

INTFLOAT=	intFloat/$(MACHTYPE)-$(OSTYPE)/intfloat.o intFloat/$(MACHTYPE)-$(OSTYPE)/intFloatImage.o intFloat/$(MACHTYPE)-$(OSTYPE)/getPhaseImage.o
INTFLOATDIRS =	 intFloat  $(PROGDIR)/cRecipes 

intfloat:	
	@for i in ${INTFLOATDIRS}; do \
		( 	echo "<<< Descending in directory: $$i >>>"; \
	                cd $$i; \
			make FLAGS=$(CCFLAGS) INCLUDEPATH=$(INCLUDEPATH) PAF=0; \
			cd $(PROGDIR); \
		); done
		gcc -m32 $(CCFLAGS1) \
                $(INTFLOAT)  $(STANDARD) $(RECIPES) $(UNWRAP)  $(CULLST) \
                -lm  -o $(BINDIR)/intfloat


