delay=${beatwatch_oopsdelay:-1}

sleep $delay
while read line; do
    echo "got a line from stdin: $line" 1>&3
done
sleep $delay
exit 0
