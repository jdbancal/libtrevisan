# Configurable settings
OPTIMISE=-O3
#DEBUG=-ggdb # -Wall -Wextra -Weffc++
#VARIANTS=-DEXPENSIVE_SANITY_CHECKS
#VARIANTS+=-DWEAKDES_TIMING
#VARIANTS+=-DUSE_NTL
OPENMP=-fopenmp -lpthread

# Platform and configuration specific optimisations
HAVE_SSE4=y
HAVE_GF2X=y

###### Nothing user-configurable below here ########
.PHONY: all clean src-pdf figures
all: extractor
BITEXTS = 1bitext_xor.o 1bitext_expander.o 1bitext_rsh.o
WDS = weakdes_gf2x.o weakdes_gfp.o weakdes_aot.o weakdes_block.o

objects = ${BITEXTS} ${WDS} timing.o primitives.o ossl_locking.o \
	  blockdes_params.o R_interp.o stream_ops.o
# Objects with a separate make target
objects.ext = generated/irreps_ntl.o generated/irreps_ossl.o

all.objects = $(objects) $(objects.ext) $(objects.r)
# TODO: We should really let gcc figure out that list so that
# we do not have to update it manually
headers = 1bitext.h debug.h timing.h weakdes_gf2x.h weakdes_gfp.h weakdes_block.h \
	  utils.hpp weakdes.h bitfield.hpp
platform=$(shell uname)
INCDIRS=-I/opt/local/include
INCDIRS+=-I../openssl/include
LIBDIRS=-L/opt/local/lib
LIBDIRS+=-L../openssl
CXXFLAGS=$(OPTIMISE) $(OPENMP) $(DEBUG) $(VARIANTS) $(INCDIRS)
ifeq ($(HAVE_SSE4),y)
CXXFLAGS+=-msse4.2 -DHAVE_SSE4
endif
LIBS=-lm -lntl -lcrypto -ltbb -lpthread -lgmp

ifeq ($(HAVE_GF2X),y)
LIBS+=-lgf2x
endif

CXXFLAGS+=-std=c++11

ifeq ($(platform),Linux)
LIBS+=-lrt -lntl
else
CXX=g++-mp-4.7
endif

# Cache the flags derived from R because they do not change across make invocations
.rcxxflags:
	@echo "Creating RCXXFLAGS"
	$(eval RCXXFLAGS := $(shell R CMD config --cppflags) \
		            $(shell echo 'Rcpp:::CxxFlags()' | R --vanilla --slave) \
		            $(shell echo 'RInside:::CxxFlags()' | R --vanilla --slave))
	@echo $(RCXXFLAGS) > .rcxxflags

.rldflags:
	@echo "Creating RLDFLAGS"
	$(eval RLDFLAGS := $(shell R CMD config --ldflags) \
			   $(shell echo 'Rcpp:::LdFlags()'  | R --vanilla --slave) \
			   $(shell echo 'RInside:::LdFlags()'  | R --vanilla --slave))
	@echo $(RLDFLAGS) > .rldflags

$(objects): %.o: %.cc %.h .rcxxflags generated/bd_r_embedd.inc \
	    generated/bitext_embedd.inc
	$(CXX) -c $(CXXFLAGS) $(shell cat .rcxxflags) $< -o $@

gen_irreps: gen_irreps.cc
	@echo "Creating gen_irreps" 
	$(CXX) $(INCDIRS) $(LIBDIRS) gen_irreps.cc -g -o gen_irreps $(LIBS) 

generated/irreps_ntl.o generated/irreps_ossl.o: gen_irreps
	@echo "Creating generated/...."
	./gen_irreps OSSL > generated/irreps_ossl.cc
	$(CXX) $(INCDIRS) generated/irreps_ossl.cc -c -g -o generated/irreps_ossl.o
	./gen_irreps > generated/irreps_ntl.cc
	$(CXX) $(INCDIRS) generated/irreps_ntl.cc -c -g -o generated/irreps_ntl.o

generated/bd_r_embedd.inc: blockdes.r
	@echo "R\"A1Y6%(" > generated/bd_r_embedd.inc
	@cat blockdes.r >> generated/bd_r_embedd.inc
	@echo ")A1Y6%\";" >> generated/bd_r_embedd.inc

generated/bitext_embedd.inc: parameters.r
	@echo "R\"A1Y6%(" > generated/bitext_embedd.inc
	@cat parameters.r >> generated/bitext_embedd.inc
	@echo ")A1Y6%\";" >> generated/bitext_embedd.inc

generated: 
	@mkdir -p generated

extractor: generated $(all.objects) extractor.cc $(headers) .rldflags .rcxxflags
	@echo "Creating EXTRACTOR"
	$(CXX) $(CXXFLAGS) $(shell cat .rcxxflags) extractor.cc $(all.objects) -o extractor \
	$(LIBDIRS) $(LIBS) $(shell cat .rldflags)

# NOTE: This reproduces some figures from the original paper
figures: | paper/pictures
	@echo "Generating figures..."
	@R CMD BATCH block_design_params.r
	@R CMD BATCH xor_params.r
	@R CMD BATCH lu_params.r

paper/pictures:
	@mkdir -p paper/pictures

src-pdf:
	enscript -E -G -j *.h *.cc *.hpp *.r \
	         -o /tmp/code.ps; ps2pdf /tmp/code.ps code.pdf
clean:
	@rm -f *.o weakdes_test 1bitext_test extractor gen_irreps
	@rm -rf generated
	@rm -f .rldflags .rcxxflags
	@rm -f *.r.Rout Rplots.pdf .RData

