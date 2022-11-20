delay=${beatwatch_oopsdelay:-1}

sleep $delay
echo DETACH 1>&3

sleep $delay
echo "This control message must be receiced." 1>&3
sleep $delay
sleep $(printf '%1.1f' $(echo "scale=1; 2 - $delay - $delay" | bc))

exit 0
