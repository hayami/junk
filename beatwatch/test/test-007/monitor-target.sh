delay=${beatwatch_oopsdelay:-1}

sleep $delay
read line2
echo "got a line2 from stdin: $line2" 1>&3
sleep $delay
exit 0
