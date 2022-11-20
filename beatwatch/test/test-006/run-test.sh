#!/bin/sh
set -e

# unset env
PATH=/usr/bin:/bin
for x in $(env | sed 's/=.*$//'); do
    case "$x" in
    HOME|LOGNAME|PATH|PWD|SHELL|SHLVL|TERM|USER) ;;
    beatwatch_*) ;;
    (*) unset $x ;;
    esac
done

rundir=${1:-.}
while [ "$1" != '--' ]; do shift; done; shift

cd $rundir

(eval "$@" 3>&1 2>stderr 1>stdout <stdin; echo $? > exitcode) \
> fd3out.log || :

../../compcat.sh	\
	exitcode	\
	stdin		\
	stdout		\
	stderr		\
	ctrl.log	\
	fd3out.log	\
	> run-test-results.txt
exit 0
