delay=${beatwatch_oopsdelay:-1}

sleep $delay
echo EXIT=234 1>&3
sleep $delay
exit 0
