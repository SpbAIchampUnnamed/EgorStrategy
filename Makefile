SHELL := /bin/bash

ifndef BUILDTYPE
	BUILDTYPE=debug
endif
BASECXXFLAGS=-std=c++20 -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-parentheses -I. -fconcepts-diagnostics-depth=10
ifeq ($(BUILDTYPE), release)
	CXXFLAGS=$(BASECXXFLAGS) -Ofast -march=native
endif
ifeq ($(BUILDTYPE), debug)
	CXXFLAGS=$(BASECXXFLAGS) -O0 -g -DLOCAL -D_GLIBCXX_DEBUG -fsanitize=address
endif
ifeq ($(BUILDTYPE), profile)
	CXXFLAGS=$(BASECXXFLAGS) -Og -g -DLOCAL -march=native
endif
VPATH = . model codegame debugging utils coro
CXX=g++
SRC=$(foreach dir,$(VPATH),$(wildcard $(dir)/*.cpp))
OBJS=$(foreach file,$(SRC),$(BUILDTYPE)/$(subst .cpp,.o,$(file)))

.SECONDEXPANSION:

.PHONY: res.zip

all: $(BUILDTYPE)/main

$(BUILDTYPE)/main: objs
	$(CXX) -o $(BUILDTYPE)/main $(CXXFLAGS) $(OBJS)

res.zip:
	rm -f res.zip
	cp Makefile build.mk
	zip -r res.zip *.cpp *.hpp build.mk CMakeLists.txt `echo $(VPATH) | cut -d ' ' -f 2-`

clean:
	rm -f $(OBJS)

test:
	echo $(OBJS)

objs: $(OBJS)

$(BUILDTYPE)/%.o: %.cpp $$(shell g++ -MM `echo $$< | cut -d '/' -f 2- | head -c -3`.cpp -I. | cut -d ' ' -f 2- | sed -e 's/\\//')
	@test -d $$(dirname $@) || mkdir -p $$(dirname $@)
	$(CXX) -c $(CXXFLAGS) -o $@ $<