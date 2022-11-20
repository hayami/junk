.PHONY:	default
default: run-test

# TEST DESCRIPTION
#   Inspects the error handling flow when the daemon() fails. The daemon() is
#   called from child_posttask(), called from cmdline(). The daemon() is
#   controlled to fail by preload.so.
#
beatwatch = ../../../obj-dynsym/beatwatch
execargs = LD_PRELOAD=../../$(testname)/preload.so			\
	$(beatwatch) --ctrl-fd 3 --ctrl-log ctrl.log $(extraopts)	\
	-- sleep 3

testname := $(shell basename $$(pwd))

ifeq ($(rundir),)
ifneq ($(set-rundir),)
rundir	:= $(shell mkdir -p ../tmp && mktemp -d "../tmp/$(testname)-XXXXXXXX")
endif
endif

MAKEFLAGS += --no-print-directory

.PHONY:	run-test run-test-
run-test: preload.so
	@test -n "$(rundir)" || $(MAKE) set-rundir=true $@-

run-test-:
	@test -n "$(rundir)"
	@test -d "$(rundir)"
	@/bin/sh run-test.sh $(rundir) -- $(execargs)
	@diff -u expected-run-test-results.txt $(rundir)/run-test-results.txt


UNAME	:= $(shell uname)
ifeq ($(UNAME),FreeBSD)
CC	= cc
else ifeq ($(UNAME),Linux)
CC	= gcc
else
CC	= cc
endif

CFLAGS	= -O2 -Wall -Wextra -Werror $(DEFS)
LDFLAGS	= -s

preload.so: preload.c
	@$(CC) $(CFLAGS) $(LDFLAGS) -fPIC -shared -o $@ $< -ldl

.PHONY:	clean
clean:
	rm -rf a.out core preload.o preload.so ../tmp/$(testname)-*

# vim: noet sw=8 sts=8
