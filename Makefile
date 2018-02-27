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
INCS = src
TESTSRC = test/src
BIN = bin
OD = obj
LIBS = -lboost_filesystem -lboost_system -lboost_serialization -ltag -lpthread $(TARG_LIBS)
GD = ./Makefile
CF = -std=c++11 -Wall -g $(TARG_CF) $(DEFS)

OBJS = 	

all: $(BIN)/hptest2 $(BIN)/semtest

.PHONY: clean

clean:
	rm -f $(OD)/*
	rm -f $(BIN)/*

$(OD)/%.o: $(SRC)/%.cpp
	g++ $(CF) -I $(INCS) -c -o $(@) $< 

$(OD)/HazardPointer.o: $(SRC)/HazardPointer.cpp $(SRC)/HazardPointer.hpp $(SRC)/Semaphore.hpp
	g++ $(CF) -c -o $(@) $< 

$(BIN)/hptest2: $(OD)/hptest2.o $(OD)/HazardPointer.o
	g++ $(CF) -o $(BIN)/hptest2 $^ $(LIBDIRS) $(LIBS)

$(OD)/hptest2.o: $(TESTSRC)/hptest2.cpp $(SRC)/HazardPointer.hpp $(SRC)/Semaphore.hpp
	g++ $(CF) -I $(INCS) -c -o $(@) $< 


$(BIN)/semtest: $(OD)/semtest.o
	g++ $(CF) -o $(BIN)/semtest $^ $(LIBDIRS) $(LIBS)

$(OD)/semtest.o: $(TESTSRC)/semtest.cpp $(SRC)/Semaphore.hpp
	g++ $(CF) -I $(INCS) -c -o $(@) $< 

