CC=gcc -O2 -Wall -lm -DMEMORY_STATS
all: hhh1 uhhh1 omp1 ancestry1 full1 hhh1_33 uhhh1_33 omp1_33 ancestry1_33 full1_33 hhh2 uhhh2 omp2 ancestry2 full2 check1 check1_33 check2 process
hhh1: hhh1.c hhh1.h main1.c lossycount.h lossycount.c hashtable.h hashtable.c prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DHHH hhh1.c main1.c lossycount.c hashtable.c prng.c alloc.c -o hhh1
uhhh1: hhh1.c hhh1.h main1.c ulossycount.h ulossycount.c hashtable.h hashtable.c prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DHHH -DUNITARY hhh1.c main1.c ulossycount.c hashtable.c prng.c alloc.c -o uhhh1
omp1: hhh1.c hhh1.h main1.c lossycount.h lossycount.c hashtable.h hashtable.c prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DHHH -DPARALLEL -fopenmp hhh1.c main1.c lossycount.c hashtable.c prng.c alloc.c -o omp1
ancestry1: ancestry1.c ancestry1.h main1.c  prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DANCESTRY ancestry1.c main1.c prng.c alloc.c -o ancestry1
full1: ancestry1.c ancestry1.h main1.c  prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DANCESTRY -DFULL_ANCESTRY ancestry1.c main1.c prng.c alloc.c -o full1
hhh1_33: hhh1.c hhh1.h main1.c lossycount.h lossycount.c hashtable.h hashtable.c prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DHHH -DNUM_MASKS=33 hhh1.c main1.c lossycount.c hashtable.c prng.c alloc.c -o hhh1_33
uhhh1_33: hhh1.c hhh1.h main1.c ulossycount.h ulossycount.c hashtable.h hashtable.c prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DHHH -DNUM_MASKS=33 -DUNITARY hhh1.c main1.c ulossycount.c hashtable.c prng.c alloc.c -o uhhh1_33
omp1_33: hhh1.c hhh1.h main1.c lossycount.h lossycount.c hashtable.h hashtable.c prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DHHH -DNUM_MASKS=33 -DPARALLEL -fopenmp hhh1.c main1.c lossycount.c hashtable.c prng.c alloc.c -o omp1_33
ancestry1_33: ancestry1.c ancestry1.h main1.c  prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DANCESTRY -DNUM_MASKS=33 ancestry1.c main1.c prng.c alloc.c -o ancestry1_33
full1_33: ancestry1.c ancestry1.h main1.c  prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DANCESTRY -DFULL_ANCESTRY -DNUM_MASKS=33 ancestry1.c main1.c prng.c alloc.c -o full1_33
hhh2: hhh2.c main2.c dim2.h lossycount.c lossycount.h prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DDIMENSION2 hhh2.c lossycount.c prng.c main2.c alloc.c -o hhh2
uhhh2: hhh2.c main2.c dim2.h ulossycount.c ulossycount.h prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DDIMENSION2 -DUNITARY hhh2.c ulossycount.c prng.c main2.c alloc.c -o uhhh2
omp2: hhh2.c main2.c dim2.h lossycount.c lossycount.h prng.h prng.c alloc.h alloc.c Makefile
	$(CC) -DDIMENSION2 -DPARALLEL -fopenmp hhh2.c lossycount.c prng.c main2.c alloc.c -o omp2
ancestry2: ancestry2.c main2.c dim2.h prng.h alloc.h alloc.c Makefile
	$(CC) -DDIMENSION2 ancestry2.c main2.c prng.c alloc.c -o ancestry2
full2: ancestry2.c main2.c dim2.h prng.h alloc.h alloc.c Makefile
	$(CC) -DDIMENSION2 -DFULL_ANCESTRY ancestry2.c main2.c prng.c alloc.c -o full2
check1: check1.cpp Makefile
	g++ -ansi -Wall -O2 check1.cpp -o check1
check1_33: check1.cpp Makefile
	g++ -ansi -Wall -O2 -DNUM_MASKS=33 check1.cpp -o check1_33
check2: check2.cpp Makefile
	g++ -ansi -Wall -O2 check2.cpp -o check2
process: process.c Makefile
	gcc -ansi -Wall -O2 process.c -o process

