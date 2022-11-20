.PHONY:	default
default: run-test-all

TARGETS-0XX	:= $(shell echo test-0[0-9]*)
TARGETS-1XX	:= $(shell echo test-1[0-9]*)
TEST-0XX	:= $(addsuffix .test,$(TARGETS-0XX))
CLEAN-0XX	:= $(addsuffix .clean,$(TARGETS-0XX))
TEST-1XX	:= $(addsuffix .test,$(TARGETS-1XX))
CLEAN-1XX	:= $(addsuffix .clean,$(TARGETS-1XX))

MAKEFLAGS += --no-print-directory


.PHONY:	run-test-all run-test-0xx run-test-1xx
run-test-all:
	$(MAKE) run-test-0xx
	$(MAKE) run-test-1xx

run-test-0xx::
	$(MAKE) -C .. build
	$(MAKE) -j run-test-0xx-jobs

run-test-1xx::
	$(MAKE) -C .. build-with-dynsym
	$(MAKE) -j run-test-1xx-jobs

.PHONY: run-test-0xx-jobs run-test-1xx-jobs
run-test-0xx-jobs: $(TEST-0XX)
run-test-1xx-jobs: $(TEST-1XX)
%.test:
	@t=$(basename $@ .run); \
	$(MAKE) -C $$t run-test \
	> /dev/null 2>&1 && ./PASS.sh ": $$t" || ./FAIL.sh ": $$t"


.PHONY:	clean run-clean-all
clean run-clean-all:
	$(MAKE) -j run-clean-all-jobs
	rm -rf tmp

.PHONY:	run-clean-all-jobs
run-clean-all-jobs: $(CLEAN-0XX) $(CLEAN-1XX)
%.clean:
	$(MAKE) -C $(basename $@ .clean) clean


# vim: noet sw=8 sts=8
