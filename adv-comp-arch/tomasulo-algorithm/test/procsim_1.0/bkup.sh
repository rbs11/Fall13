for ((J=1; J<=3; J++))
do		 
	./procsim -j $J<traces/$FILE
done
for ((J=1; J<=3; J++))
do		 
	./procsim -k $J<traces/$FILE
done
for ((J=1; J<=3; J++))
do		 
	./procsim -l $J<traces/$FILE
done
for ((J=2; J<=8; J=$J*2))
do		 
	./procsim -f $J<traces/$FILE
done
for ((J=2; J<=8; J=$J*2))
do		 
	./procsim -m $J<traces/$FILE
done
for ((S=8; S<=128; (S=$S*4)))
do		 
	./procsim -r $S<traces/$FILE
done