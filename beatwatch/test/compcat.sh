#!/bin/sh

time_script() {
    d='[0-9][0-9]*'
    d2='[0-9][0-9]'
    d3='[0-9][0-9][0-9]'
    d4='[0-9][0-9][0-9][0-9]'
    regex="${d}-${d2}-${d2} ${d2}:${d2}:${d2}[.]${d3} [(][+-]${d4}[)]"

    printf "s/:${regex}\t/:(time) /"
}

pid_script() {
    cat "$@" 2> /dev/null			\
    | sed -E -e 's/PID=/\nPID=/g'		\
    | sed -E -n 's/^PID=-?([0-9][0-9]*).*/\1/p'	\
    | sort -n -u | (num=100; while  read pid; do
        num=$(($num + 1))
        echo $pid $num
    done) | awk '{ print length($1), $1, $2 }'	\
    | sort -n -r | cut -d ' ' -f 2-		\
    | while read p1 p2; do
        printf ' -e s/PID=%d/PID=00%d/g\n' $p1 $p2
        printf ' -e s/PID=-%d/PID=-00%d/g\n' $p1 $p2
    done
}

pid_script=$(pid_script "$@")

for file in "$@"; do
    if [ -e $file ]; then
        if [ -s $file ]; then
            grep --with-filename -E -e '.*' $file		\
                | sed -E -e "$(time_script)" $pid_script	\
                | sed -E -e 's/^([^:]*):/\1: /'
        else
            echo "${file}: (empty)"
        fi
    else
            echo "${file}: (not found)"
    fi
done

exit 0

# vim: et sw=4 sts=4
