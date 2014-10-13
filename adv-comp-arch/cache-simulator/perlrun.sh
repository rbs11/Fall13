for (( c=14; c<=15; c++ ))
do
	for (( b=7; b<=15; b++ ))
	do		 
		for (( s=0; s<= 2; s++ ))
		do 
			for (( C=16; C<=17; C++ ))
			do 
				for ((B=7; B<=17; B++ ))
				do
					for ((S=0; S<=8; S++ ))
					do 	
					./cachesim -c $c -b $b -s $s -C $C -B $B -S $S -k 4 <traces/perlbench.trace
					done					
				done
			done
		done
	done
done
