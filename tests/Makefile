MAKEFLAGS = -j8
CFLAGS = -Wall -std=c99 -s -O2
LIBS = -ldnsapi -lws2_32 -lwininet -lshlwapi

ifneq ($(OS),Windows_NT)
	CC = i586-mingw32msvc-cc
else
	CC = gcc
endif

# Standalone MSVC regression needs generated production snippets; see its README.
TESTS = $(filter-out api-call-metrics.c,$(wildcard *.c))
TESTSEXE = $(TESTS:.c=.exe)

# please build all the object files using the main Makefile (in the parent
# directory)
CUCKOOOBJ := $(wildcard ../objects/*.o)
CUCKOOOBJ += $(wildcard ../objects/bson/*.o)
CUCKOOOBJ += $(wildcard ../objects/distorm3.2/*.o)

all: $(TESTSEXE)

%.exe: %.c $(CUCKOOOBJ) $(DISTORM3OBJ)
	$(CC) $(CFLAGS) -I../distorm3.2-package/include -I.. -o $@ $^ $(LIBS)

clean:
	rm -f $(TESTSEXE)
