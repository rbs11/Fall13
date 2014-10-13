#echo "GCC"
FILE="mcf.100k.trace"
for ((J=1; J<=3; J++))
do		 
	for ((K=1; K<=3; K++ ))
	do 
		for ((L=1; L<=3; L++ ))
		do
			for ((M=2; M<=8; (M=$M*2)))
			do 
				for ((f=2; f<=8; (f=$f*2)))
				do
					for ((S=8; S<=128; (S=$S*4)))
					do
						P=0
						./procsim -r $S -j $J -k $K -l $L -m $M -f $f<traces/$FILE
					done
				done
			done					
		done
	done
done
