.PHONY:	default
default: build
	@: Supress "Nothing to be done for '...'" message by make

UNAME   := $(shell uname)
ifeq ($(UNAME),FreeBSD)
CC	= cc
else ifeq ($(UNAME),Linux)
CC	= gcc
else
CC	= cc
endif

CFLAGS	= -O2 -Wall -Wextra -Werror $(DEFS)
LDFLAGS = -s
OBJS	=		\
	cmdline.o	\
	ctrl.o		\
	errorpf.o	\
	execfunc.o	\
	fork.o		\
	main.o		\
	sigmisc.o	\
	watchdog.o
SRCPATH	=

MAKEFLAGS += --no-print-directory

beatwatch: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o:	$(SRCPATH)%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

.PHONY:	clean
clean:
	rm -rf beatwatch *.o obj obj-*

.PHONY:	build
build:
	mkdir -p obj
	cd obj && \
	$(MAKE) -f ../Makefile SRCPATH=../ beatwatch

.PHONY:	build-with-debug
build-with-debug:
	mkdir -p obj-debug
	cd obj-debug && \
	$(MAKE) -f ../Makefile SRCPATH=../ CFLAGS=-g LDFLAGS= beatwatch

.PHONY:	build-with-arg-parser
build-with-arg-parser:
	mkdir -p obj-arg-parser
	cd obj-arg-parser && \
	$(MAKE) -f ../Makefile SRCPATH=../ DEFS=-DDEBUG_ARG_PARSER beatwatch

.PHONY:	build-with-dynsym
build-with-dynsym:
	mkdir -p obj-dynsym
	cd obj-dynsym && \
	$(MAKE) -f ../Makefile SRCPATH=../ LDFLAGS='-s -rdynamic' beatwatch

# vim: noet sw=8 sts=8
