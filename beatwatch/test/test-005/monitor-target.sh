delay=${beatwatch_oopsdelay:-1}

sleep $delay
echo 'This line goes to stdout'
echo 'This line goes to stderr' 1>&2
echo 'This line goes to fd 3'   1>&3
sleep $delay
exit 0
