#!/bin/bash

# use hello as default program
if [[ -n "$1" ]]; then
	program=$1
else
	program="hello"
fi

#servers=("cpuserver2" "cpuserver3" "cpuserver4" "cpuserver5" "cpuserver6")
servers=("yyp196" "yyp192")

for server in "${servers[@]}"; do
	# get pid
	pid=$(ssh $server "cd jianode/$program && lsof -w $program" | awk 'NR==2 {print $2}')
	
	# pid non-null
	if [ -n "$pid" ]; then
		echo "process $pid in running on $server"
		ssh $server "kill -9 $pid && echo \"process $pid has been killed\""
	fi
	echo "$program on $server is ok"
done
