delay=${beatwatch_oopsdelay:-1}

sleep $delay
echo TESTPID=$$ 1>&3
sleep 3
exit 0
