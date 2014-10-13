echo "ASTAR"
for (( b=4; b<=9; b++ ))
do		 
	for (( s=0; s<= 1; s++ ))
	do 
		for ((B=11; B<=15; B++ ))
		do
			for ((S=0; S<=4; S++ ))
			do 	
				./cachesim -c 15 -b $b -s $s -C 17 -B $B -S $S -k 0 <traces/bzip2.trace
			done					
		done
	done
done
