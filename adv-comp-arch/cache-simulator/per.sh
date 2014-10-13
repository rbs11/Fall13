echo "ASTAR"
for (( b=5; b<=9; b++ ))
do		 
	for (( s=0; s<= 2; s++ ))
	do 
		for ((B=5; B<=10; B++ ))
		do
			for ((S=3; S<=8; S++ ))
			do 	
				./cachesim -c 15 -b $b -s $s -C 17 -B $B -S $S -k 4 <traces/perlbench.trace
			done					
		done
	done
done
