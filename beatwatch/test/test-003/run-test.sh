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

. ./run-test.shfunc

rundir=${1:-.}
while [ "$1" != '--' ]; do shift; done; shift

cd $rundir

(eval "$@" 3>&1 2>stderr 1>stdout </dev/null; echo $? > exitcode) \
| while read line; do
    case "$line" in
    TESTPID=*)
        mtarget_pid=${line#TESTPID=}
        mtarget_pgid=$(ps -p $mtarget_pid -o pgid= | sed -e 's/ //g')
        mtarget_sid=$(ps -p $mtarget_pid -o sid=   | sed -e 's/ //g')
        mtarget_ppid=$(ps -p $mtarget_pid -o ppid= | sed -e 's/ //g')
        mtarget_cmdname=$(psmatch PID $mtarget_pid COMMAND)

        watchdog_pid=$mtarget_sid
        watchdog_pgid=$(ps -p $watchdog_pid -o pgid= | sed -e 's/ //g')
        watchdog_sid=$(ps -p $watchdog_pid -o sid=   | sed -e 's/ //g')
        watchdog_ppid=$(ps -p $watchdog_pid -o ppid= | sed -e 's/ //g')
        watchdog_cmdname=$(psmatch PID $watchdog_pid COMMAND)

        # check the number of same PGID
        num=$(psmatch PGID $mtarget_pgid | wc -l | sed -e 's/ //g')
        [ $num -eq 2 ] && printf "PASS: " || printf "FAIL: "
        echo "number of same PGID is $num (shoud be 2)"	# /bin/sh and sleep

        # check the number of same SID
        sid=$(ps -p $mtarget_pid -o sid=)
        num=$(psmatch SID $mtarget_sid | wc -l | sed -e 's/ //g')
        [ $num -eq 3 ] && printf "PASS: " || printf "FAIL: "
        echo "number of same SID is $num (should be 3)"	# beatwatch, /bin/sh and sleep

        # check tty names of same SID
        psmatch SID $mtarget_sid TT | while read tty; do
            case "$tty" in
            '?'|-) ;;
            *) echo "FAIL: unexpected tty name: $tty" ;;
            esac
        done

        # check PPID of beatwatch (watchdog)
        [ $watchdog_ppid -eq 1 ] && printf "PASS: " || printf "FAIL: "
        echo "PPID of beatwatch is $watchdog_ppid (should be 1)"

        # check PID of beatwatch (watchdog)
        [ $watchdog_pid -eq $mtarget_ppid ] && printf "PASS: " || printf "FAIL: "
        echo "PID of beatwatch is PPID of monitoring-target"

        # check PGID of beatwatch (watchdog)
        [ $watchdog_pgid -ne $mtarget_pgid ] && printf "PASS: " || printf "FAIL: "
        echo "PGID of beatwatch is not same as PGID of monitoring-target"

        # check SID of beatwatch (watchdog)
        [ $watchdog_sid -eq $mtarget_sid ] && printf "PASS: " || printf "FAIL: "
        echo "SID of beatwatch is same as SID of monitoring-target"

        # check PPID of monitoring-target
        [ $mtarget_ppid -eq $watchdog_pid ] && printf "PASS: " || printf "FAIL: "
        echo "PPID of monitoring-target is same as PID of beatwach"

        # check PID of monitoring-target
        [ $mtarget_pid -ne $watchdog_pid ] && printf "PASS: " || printf "FAIL: "
        echo "PID of monitoring-target is not same as PID of beatwach"

        # check PGID of monitoring-target
        [ $mtarget_pgid -ne $watchdog_pgid ] && printf "PASS: " || printf "FAIL: "
        echo "PGID of monitoring-target is not same as PGID of beatwatch"

        # check beatwatch (watchdog) COMMAND name where PID == SID
        set -- $watchdog_cmdname
        cmdname=${1##*/}
        [ "$cmdname" = "beatwatch" ] && printf "PASS: " || printf "FAIL: "
        echo "COMMAND name where PID == SID is '$cmdname' (should be 'beatwatch')"

        # check monitorinig-target COMMAND name where PID == PGID
        set -- $mtarget_cmdname
        cmdname=$1
        [ "$cmdname" = "/bin/sh" ] && printf "PASS: " || printf "FAIL: "
        echo "COMMAND name where PID == PGID is '$cmdname' (should be '/bin/sh')"

        ;;
    esac
done > fd3out.log || :

../../compcat.sh	\
	exitcode	\
	stdout		\
	stderr		\
	ctrl.log	\
	fd3out.log	\
	> run-test-results.txt
exit 0
