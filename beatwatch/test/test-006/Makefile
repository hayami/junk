.PHONY:	default
default: run-test

# TEST DESCRIPTION
#   The monitor target (monitor-target.sh) reads stdin without DETACH.
#
beatwatch = ../../../obj/beatwatch
execargs = $(beatwatch) --debug						\
	--ctrl-fd 3 --ctrl-log ctrl.log $(extraopts)			\
	-- /bin/sh monitor-target.sh

testname := $(shell basename $$(pwd))

ifeq ($(rundir),)
ifneq ($(set-rundir),)
rundir	:= $(shell mkdir -p ../tmp && mktemp -d "../tmp/$(testname)-XXXXXXXX")
endif
endif

MAKEFLAGS += --no-print-directory

.PHONY:	run-test run-test-
run-test:
	@test -n "$(rundir)" || $(MAKE) set-rundir=true $@-

run-test-:
	@test -n "$(rundir)"
	@test -d "$(rundir)"
	@cp stdin $(rundir)
	@cp monitor-target.sh $(rundir)
	@/bin/sh run-test.sh $(rundir) -- $(execargs)
	@diff -u expected-run-test-results.txt $(rundir)/run-test-results.txt

.PHONY:	clean
clean:
	rm -rf ../tmp/$(testname)-*

# vim: noet sw=8 sts=8
