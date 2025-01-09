#!/bin/bash
# shellcheck disable=SC2086

programs=("ep" "hello" "is" "lu" "mm" "pi" "sor" "tsp" "water")
#servers=("cpuserver2" "cpuserver3" "cpuserver4" "cpuserver5" "cpuserver6")
servers=("yyp196" "yyp192")

for server in "${servers[@]}"; do
	if [ -n "$1" ]; then
		# get pid
		pid=$(ssh $server "cd jianode/$1 && lsof -w $1" | awk 'NR==2 {print $2}')
	
		# pid non-null
		if [ -n "$pid" ]; then
			echo "process $pid in running on $server"
			ssh $server "kill -9 $pid && echo \"process $pid has been killed\""
		fi
		echo "$1 on $server is ok"
	else
		for pg in "${programs[@]}"; do
					
			# get pid
			pid=$(ssh $server "cd jianode/$pg && lsof -w $pg" | awk 'NR==2 {print $2}')
	
			# pid non-null
			if [ -n "$pid" ]; then
				echo "process $pid in running on $server"
				ssh $server "kill -9 $pid && echo \"process $pid has been killed\""
			fi
			echo "$pg on $server is ok"
		done
	fi
done
