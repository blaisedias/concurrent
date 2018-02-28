#
#
TARG_ARCH=$(shell uname -m)

ifeq ($(TARG_ARCH),x86_64)
	LIBDIRS = -L/usr/lib/x86_64-linux-gnu
	TARG_LIBS = 
	TARG_CF = 
endif
ifeq ($(TARG_ARCH),i686)
	LIBDIRS = 
	TARG_LIBS = 
	TARG_CF = 
endif
ifeq ($(TARG_ARCH),armv6l)
	LIBDIRS = -L/usr/lib/arm-linux-gnueabihf
	TARG_LIBS =
	TARG_CF =
endif

DEFS =
SRC = src
INCLUDES = -I src
TESTSRC = test/src
BIN = bin
OD = obj
LIBS = -lboost_filesystem -lboost_system -lboost_serialization -ltag -lpthread $(TARG_LIBS)
GD = ./Makefile
CF = -std=c++14 -Wall -g $(TARG_CF) $(DEFS)

OBJS = 	

all: $(BIN)/hptest2 $(BIN)/SemTest $(BIN)/thread_test $(BIN)/semaphore_test

.PHONY: clean

clean:
	rm -f $(OD)/*
	rm -f $(BIN)/*

#{
Makefile.deps: $(SRC)/* $(TESTSRC)/*
	./mkdeps.py $(SRC) $(TESTSRC) -o '$(OD)'
	touch Makefile.deps

include Makefile.deps
#}
# Alternative makefile way of doing above.
#
#https://www.gnu.org/software/make/manual/make.html#Automatic-Prerequisites
#%.d: %.c
#        @set -e; rm -f $@; \
#         $(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
#         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
#         rm -f $@.$$$$
#
#include $(sources:.c=.d)
#
$(OD)/%.o: $(SRC)/%.cpp $(GD)
	g++ $(CF) -c -o $(@) $< $(INCLUDES)

$(OD)/%.o: $(SRC)/%.c $(GD)
	g++ $(CF) -c -o $(@) $< $(INCLUDES)

$(OD)/%.o: $(TESTSRC)/%.cpp $(GD)
	g++ $(CF) -c -o $(@) $< $(INCLUDES)

$(OD)/%.o: $(TESTSRC)/%.c $(GD)
	g++ $(CF) -c -o $(@) $< $(INCLUDES)


$(BIN)/hptest2: $(OD)/hptest2.o $(OD)/HazardPointer.o
	g++ $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)


$(BIN)/SemTest: $(OD)/SemTest.o
	g++ $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)


$(BIN)/thread_test: $(OD)/thread_test.o $(OD)/rwlocktest2.o $(OD)/pilocktest.o \
	$(OD)/bdrwlock.o $(OD)/bdfutex.o $(OD)/bdlock.o 
	g++ $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)

$(BIN)/semaphore_test:  $(OD)/semaphore_test.o $(OD)/semaphore.o $(OD)/bdfutex.o 
	g++ $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)

