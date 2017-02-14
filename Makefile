####################################################################################
#	Notes on Makefile -- Andrea Rommal 7/18/2013
#
#	IN ORDER TO RUN THE LIBRARY GF2X, THE COMMAND "export LD_LIBRARY_PATH="/(PATH_TO_LIBRARY)" MUST BE USED
#	BEFORE "make". I HAVE LOCATED THE LIBRARY IN "/home/amr3/lib/gf2x-1.1/lib/"
#	This is necessary because these are dynamic libs & are accessed at run time not just at compile time
#	For this dir & ver use: export LD_LIBRARY_PATH="../lib/gf2x-1.1/lib/"
#
#	Depending on the computer settings, HAVE_SSE4 and HAVE_GF2X y/n will need to be changed.
#	CXXFLAGS+=-std=gnu++11 may need to be changed to CXXFLAGS+=-std=gnu++0x if C++11 is not available.
#
#	Since this version of the Trevisan code includes only RSH, GFP, and GF2X, several libraries
#	previously required are no longer needed and are not included in this makefile.
#	The libraries needed to run this isolated version of Trevisan are:
#
#	OpenSSL toolkit and library	------ for -lssl  & -lcrypto library commands and header files
#				note: this version MUST have both BN_GF2m_mod_mul_arr and BN_GF2m_add as defined functions
#	GMP library					------ for -lgmp library command and header files
#	NTL library					------ for -lntl library command  and header files
#	GF2X library (if using gf2x)------ for -lgf2x library command and header files
#
#	note: need to mod PATH via:
#		export LD_LIBRARYPATH="../lib/gf2x-1.1/lib"
#	      default ASCII output file is:
#	      	output_m.txt
#####################################################################################
#	To run the block weak design, the following libraries need to be added:
#	
#	Rcpp library 				----- for -lRcpp library command and header files
#	RInside library				----- for -lRinside library command and header files
#	TBB library					----- for -ltbb library command and header files
#			note: -lRcpp, -lRinside, and -ltbb library commands may need be added with the others at line "LIBS="
#
#	Uncomment the commented-out lines of code about .rcxxflags, .rldflags, $(objects), and extractor:.
#	Comment out the current "extractor: " lines.
#
#	Also, weakdes_block.o should be added to the line "WD=", weakdes_block.h added to line "headers =",
#	and blockdes_params.o & ossl_locking.o added to "objects =" after the files 
#	(including the .r files) are added to the work folder.
#
#	The following files from Mauerer will be needed:	block_params.cc, block_params.h,
#			R_interp.h, R_interp.cc, weakdes_block.cc, weakdes_block.h, block_design_params.r,
#			blockdes.r, parameters.r
#####################################################################################

# Configurable settings
OPTIMISE=-O3  #-Wall # -O3
#DEBUG=-ggdb # -Wall -Wextra -Weffc++
#VARIANTS=-DEXPENSIVE_SANITY_CHECKS
#VARIANTS+=-DWEAKDES_TIMING
#VARIANTS+=-DUSE_NTL

# Platform and configuration specific optimisations
HAVE_SSE4=y
HAVE_GF2X=y

###### Nothing user-configurable below here ########
.PHONY: all clean paper src-pdf figures notes
all: extractor
BITEXTS = 1bitext_rsh.o
WDS = weakdes_gf2x.o weakdes_gfp.o

objects = ${BITEXTS} ${WDS} timing.o primitives.o stream_ops.o
# Objects with a separate make target
objects.ext = generated/irreps_ntl.o generated/irreps_ossl.o

all.objects = $(objects) $(objects.ext) $(objects.r)
# TODO: We should really let gcc figure out that list so that
# we do not have to update it manually
headers = 1bitext.h debug.h timing.h weakdes_gf2x.h weakdes_gfp.h  \
	  utils.hpp weakdes.h bitfield.hpp prng.hpp 1bitext_rsh.h primitives.h stream_ops.h
platform=$(shell uname)
 INCDIRS=-I../bin/include
 INCDIRS+=-I../lib/openssl-1.0.1e/include
 INCDIRS+=-I../bin/include/NTL/ntl-6.0.0/include
 INCDIRS+=-I../lib/gf2x-1.1/include
 INCDIRS+=-I/opt/local/include

 LIBDIRS=-L/opt/local/lib
 LIBDIRS+=-L../lib/openssl-1.0.1e
 LIBDIRS+=-L../bin/include/lib
 LIBDIRS+=-L../lib/gf2x-1.1/lib
 LIBDIRS+=-L../bin/include/NTL/ntl-6.0.0/src
 CXXFLAGS=$(OPTIMISE) $(OPENMP) $(DEBUG) $(VARIANTS) $(INCDIRS)

ifeq ($(HAVE_SSE4),y)
CXXFLAGS+=-msse4.2 -DHAVE_SSE4
endif

LIBS=-lgmp -lm -lntl -lssl -lcrypto

ifeq ($(HAVE_GF2X),y)
LIBS+= -lgf2x
endif

ifeq ($(platform),Linux)
CXXFLAGS+=-std=gnu++0x
LIBS+=-lrt
else
CXXFLAGS+=-std=c++11
CXX=g++-mp-4.7
endif

# # Cache the flags derived from R because they do not change across make invocations
# .rcxxflags:
# @echo "Creating RCXXFLAGS"
# $(eval RCXXFLAGS := $(shell R CMD config --cppflags) \
# $(shell echo 'Rcpp:::CxxFlags()' | R --vanilla --slave) \
# $(shell echo 'RInside:::CxxFlags()' | R --vanilla --slave))
# @echo $(RCXXFLAGS) > .rcxxflags

# .rldflags:
# @echo "Creating RLDFLAGS"
# $(eval RLDFLAGS := $(shell R CMD config --ldflags) \
# $(shell echo 'Rcpp:::LdFlags()'  | R --vanilla --slave) \
# $(shell echo 'RInside:::LdFlags()'  | R --vanilla --slave))
# @echo $(RLDFLAGS) > .rldflags

# $(objects): %.o: %.cc %.h .rcxxflags generated/bd_r_embedd.inc \
# generated/bitext_embedd.inc
# $(CXX) -c $(CXXFLAGS) $(shell cat .rcxxflags) $< -o $@
	
	
gen_irreps: gen_irreps.cc
	@echo "Creating gen_irreps" 
	$(CXX) $(INCDIRS) $(LIBDIRS) gen_irreps.cc -g -o gen_irreps -lntl -lgf2x 

generated/irreps_ntl.o generated/irreps_ossl.o: gen_irreps
	@echo "Creating generated/...."
	./gen_irreps OSSL > generated/irreps_ossl.cc
	$(CXX) $(INCDIRS) generated/irreps_ossl.cc -c -g -o generated/irreps_ossl.o
	./gen_irreps > generated/irreps_ntl.cc
	$(CXX) $(INCDIRS) generated/irreps_ntl.cc -c -g -o generated/irreps_ntl.o
	
# generated/bd_r_embedd.inc: blockdes.r							//ROMMAL EDIT 8-5-13, commented out this block, and the block below
# @echo "R\"A1Y6%(" > generated/bd_r_embedd.inc
# @cat blockdes.r >> generated/bd_r_embedd.inc
# @echo ")A1Y6%\";" >> generated/bd_r_embedd.inc

# generated/bitext_embedd.inc: parameters.r
# @echo "R\"A1Y6%(" > generated/bitext_embedd.inc
# @cat parameters.r >> generated/bitext_embedd.inc
# @echo ")A1Y6%\";" >> generated/bitext_embedd.inc

extractor: $(all.objects) extractor.cc $(headers)
	@echo "Creating EXTRACTOR"
	$(CXX) $(CXXFLAGS) -g $(INCDIRS) extractor.cc $(all.objects) -o extractor \
	$(LIBDIRS) $(LIBS)
	
# extractor: $(all.objects) extractor.cc $(headers) .rldflags .rcxxflags
# @echo "Creating EXTRACTOR"
# $(CXX) $(CXXFLAGS) $(shell cat .rcxxflags) extractor.cc $(all.objects) -o extractor \
# $(shell cat .rldflags) $(LIBDIRS) $(LIBS)
	
# NOTE: This is separated from the paper target on purpose. Generating the
# figures takes long compared to TeXing the paper, and the inputs rarely change.
# A proper solution would be to write the paper in Sweave and use cacheSweave,
# but for the moment, the extra complexity does not seem worth it.
# NOTE: Did not bother to check if the source files are more up-to-date than
# the pictures. They are always (re)generated when this target is run


src-pdf:
	enscript -E -G -j *.h *.cc *.hpp *.r \
	         -o /tmp/code.ps; ps2pdf /tmp/code.ps code.pdf
clean:
	@rm -f *.o weakdes_test 1bitext_test extractor
	@rm -rf generated/*
#	#@rm -f .rldflags .rcxxflags
#	@$(MAKE) clean -C paper
