.PHONY:	default
default: run-test

# TEST DESCRIPTION
#   The monitor target (monitor-target.sh) send EXIT=234 to the control channel
#   without DETACH. The EXIT=234 instructs the beatwatch (watchdog) and the
#   beatwatch (cmdline) to recommend using 234 as the exit code when terminate
#   after that.
#
beatwatch = ../../../obj/beatwatch
execargs = $(beatwatch) --debug --ctrl-fd 3 --ctrl-log ctrl.log		\
	--on-exit-script \''						\
		ret=$$?;						\
		echo This line is stdout: ret=$$ret;			\
		echo This line is stderr: ret=$$ret 1>&2;		\
	'\' $(extraopts)						\
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
	@cp monitor-target.sh $(rundir)
	@/bin/sh run-test.sh $(rundir) -- $(execargs)
	@diff -u expected-run-test-results.txt $(rundir)/run-test-results.txt

.PHONY:	clean
clean:
	rm -rf ../tmp/$(testname)-*

# vim: noet sw=8 sts=8
