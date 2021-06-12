ARCH = $(shell uname)

CC = g++ -Wall -Wno-write-strings

PROGRAMS = encode decode 

ARITHM_OBJS = \
  AdaptiveModel.o \
  ArithmeticDecoder.o \
  ArithmeticEncoder.o 

PNM_OBJS = \
  pnm.o 

MISC_OBJS = \
  CommandLine.o

IMAGECODEC_OBJS = \
  Image.o \
  StatisticalModel.o \
  BinaryModeModel.o \
  ImageCodec.o \
  CALIC.o 

#  InterframeCALIC.o
#  ISOCalic.o \
#  PredictOnlyCodec.o 

OBJS = $(ARITHM_OBJS) $(PNM_OBJS) $(MISC_OBJS) $(IMAGECODEC_OBJS)

INCDIR = .

.PHONY: debug release clean

debug: clean
debug: CFLAGS = -g -I$(INCDIR) -D$(ARCH) -DANALYSIS
debug: $(PROGRAMS)

release: clean
release: CFLAGS = -O3 -I$(INCDIR) -D$(ARCH) -DNDEBUG 
release: $(PROGRAMS)

profiling: clean
profiling: CFLAGS = -O3 -I$(INCDIR) -D$(ARCH) -DNDEBUG -pg
profiling: LINK_FLAGS = -pg
profiling: $(PROGRAMS)
  

.SUFFIXES: .cc .
.cc.o:
	$(CC) -c $(INCLUDE) $(CFLAGS) $<

all: release

encode: $(OBJS) encode.o 
	$(CC) -o $@ $(LINK_FLAGS) encode.o $(OBJS) -lm

decode: $(OBJS) decode.o
	$(CC) -o $@ $(LINK_FLAGS) decode.o $(OBJS) -lm

clean:
	rm -rf *.o $(PROGRAMS) core

