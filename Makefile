ROOTCFLAGS   := $(shell root-config --cflags)
ROOTLIBS     := $(shell root-config --libs)
ROOTGLIBS    := $(shell root-config --glibs)
ROOTINCDIR   := $(DESTDIR)$(shell root-config --incdir)
ROOTLIBDIR   := $(DESTDIR)$(shell root-config --libdir)

CC           = gcc
CXX          = g++

CXXFLAGS     += -g -Wall -fPIC -I.
CFLAGS       += -g -Wall -fPIC -I.
CXXFLAGS     += $(ROOTCFLAGS)
LDFLAGS      += $(ROOTLIBS)

# Dictionaries
DICTS += ISSFile
DICTS += ISSBuffer
DICTS += ISSWord
DICTS += ISSHit

# Libraries

LIB1 = libANISS.so
LIB1OBJS += ISSFile.Dict.o
LIB1OBJS += ISSBuffer.Dict.o
LIB1OBJS += ISSWord.Dict.o
LIB1OBJS += ISSHit.Dict.o

# Header files
HDR += ISSFile.hh
HDR += ISSBuffer.hh
HDR += ISSWord.hh
HDR += ISSHit.hh
HDR += ISSHeader.hh

DICT_CC = $(foreach D, $(DICTS), $(D).Dict.cc)
DICT_H = $(foreach D, $(DICTS), $(D).Dict.h)

#all: $(LIB1) doc/libANISS.pdf

$(LIB1): $(LIB1OBJS)
	 $(CXX) -shared -Wl,-soname,$(LIB1) -o $@ $(LIB1OBJS)

%.Dict.cc: %.hh
	rootcint -f $@ -c $<

proper:
	rm -f *~ *.o $(DICT_CC) $(DICT_H)

clean:
	rm -f *~ *.o $(LIB1) $(DICT_CC) $(DICT_H) AutoDict* \
	G__auto*LinkDef.h examples/*.so examples/*.d

install: $(LIB1)
	install -m 755 -d $(ROOTLIBDIR)
	install -m 755 $(LIB1) $(ROOTLIBDIR)
	install -m 644 libANISS.rootmap $(ROOTLIBDIR)
	install -m 755 -d $(ROOTINCDIR)
	install -m 644 $(HDR) $(ROOTINCDIR)

deinstall:
	rm -f $(ROOTLIBDIR)/$(LIB1)
	for i in $(HDR) ; do rm -f $(ROOTINCDIR)/$$i ; done
	rm -rf $(ROOTINCDIR)/mbsio/

