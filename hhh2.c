/*
Implementation of the 2-dimensional Hierarchical Heavy Hitters algorithm (HHH) for IP addresses
-Thomas Steinke (tsteinke@seas.harvard.edu) 2010-10-06
*/

#include <stdlib.h>
#include <stdio.h>

//for debugging purposes only
#include <assert.h> 
//#include <set> //debug
//#include <utility> //debug

#ifdef UNITARY
#include "ulossycount.h"
#define COUNTERSIZE k
#define COUNTERS items
#define COUNT parentg->count
#define LCL_type LCU_type
#define LCL_Init LCU_Init
#define LCL_Destroy LCU_Destroy
#define LCL_Update(x,y,z) LCU_Update(x,y)
#define LCL_PointEstUpp LCU_PointEstUpp
#define LCL_PointEstLow LCU_PointEstLow
#else
#include "lossycount.h"
#define COUNTERSIZE size
#define COUNTERS counters
#define COUNT count
#endif
//#include "hashtable.h"

#include "dim2.h"
#include "alloc.h"

#ifndef DIMENSION2
#error This is the two-dimensional implementation 
#endif


#define D(x...) fprintf(stderr, x) //debug
#define P(x...) fprintf(stderr, x)
#define PIP(item) fprintf(stderr, "%3d.%3d.%3d.%3d", (int)(255&((item) >> 24)), (int)(255&((item) >> 16)), (int)(255&((item) >> 8)), (int)(255&((item) >> 0)))

//#define NUM_COUNTERS 16 //number of masks
//#define MAX_DESCENDANTS 512 //maximum number of direct descendents of a given ip pair

int min(int a, int b) {return (a <= b ? a : b);}
int max(int a, int b) {return (a >= b ? a : b);}

//The counters
LCL_type * counters[NUM_COUNTERS];
//The masks associated with the counters
//Note that we must ensure that they are in increasing order of generality
/*LCLitem_t masks[NUM_COUNTERS] = {
	//255.255.255.255
	0xFFFFFFFFFFFFFFFFull, //255.255.255.255
	0xFFFFFF00FFFFFFFFull, //255.255.255.0
	0xFFFF0000FFFFFFFFull, //255.255.0.0
	0xFF000000FFFFFFFFull, //255.0.0.0

	//255.255.255.0
	0xFFFFFFFFFFFFFF00ull, //255.255.255.255
	0xFFFFFF00FFFFFF00ull, //255.255.255.0
	0xFFFF0000FFFFFF00ull, //255.255.0.0
	0xFF000000FFFFFF00ull, //255.0.0.0

	//255.255.0.0
	0xFFFFFFFFFFFF0000ull, //255.255.255.255
	0xFFFFFF00FFFF0000ull, //255.255.255.0
	0xFFFF0000FFFF0000ull, //255.255.0.0
	0xFF000000FFFF0000ull, //255.0.0.0

	//255.0.0.0
	0xFFFFFFFFFF000000ull, //255.255.255.255
	0xFFFFFF00FF000000ull, //255.255.255.0
	0xFFFF0000FF000000ull, //255.255.0.0
	0xFF000000FF000000ull  //255.0.0.0
};*/

int leveleps[NUM_COUNTERS] = {
64, 56, 48, 40, 32,
56, 48, 40, 32, 24,
48, 40, 32, 24, 16,
40, 32, 24, 16,  8,
32, 24, 16,  8,  0
};

double dblmax(double a, double b) {return (a >= b ? a : b);}

double twototheminus(int k) {
	double ans = 1;
	while (k > 0) {ans /= 2; k--;}
	return ans;
}

//initialise
void init(double epsilon) {
	int i;
	for (i = 0; i < NUM_COUNTERS; i++)
		counters[i] = LCL_Init(dblmax(epsilon, twototheminus(leveleps[i])));
}

//deinitialise
void deinit() {
	int i;
	for (i = 0; i < NUM_COUNTERS; i++)
		LCL_Destroy(counters[i]);
}

#ifndef PARALLEL
//update an input
void update(LCLitem_t item, int count) {
	int i;
	for (i = 0; i < NUM_COUNTERS; i++) {
		LCL_Update(counters[i], item & masks[i], count);
		//P("update [%2d] ", i); PIP((item & masks[i]) >> 32); P(" "); PIP(item & masks[i]); P("\n");
	}
}
#else
//update an input sequence
void update(LCLitem_t * item, int count) {
	int i, j;
	#pragma omp parallel for private(j)
	for (i = 0; i < NUM_COUNTERS; i++)
		for (j = 0; j < count; j++)
			LCL_Update(counters[i], item[j] & masks[i], 1);
}
#endif

/*/struct to store a heavy hitter output
typedef struct heavyhitter {
	LCLitem_t item; //The item
	int mask; //The mask id
	int upper, lower; //Upper and lower count bounds
	//int s, t;//debug
} HeavyHitter;*/

//we want to sort heavyhitters
int cmpHH(const void * lhs, const void * rhs) {
	if (((const HeavyHitter*) lhs)->item > ((const HeavyHitter*) rhs)->item) return 1;
	if (((const HeavyHitter*) lhs)->item < ((const HeavyHitter*) rhs)->item) return -1;
	if (((const HeavyHitter*) lhs)->mask > ((const HeavyHitter*) rhs)->mask) return 1;
	if (((const HeavyHitter*) lhs)->mask < ((const HeavyHitter*) rhs)->mask) return -1;
	if (((const HeavyHitter*) lhs)->upper != ((const HeavyHitter*) rhs)->upper) return ((const HeavyHitter*) lhs)->upper - ((const HeavyHitter*) rhs)->upper;
	return ((const HeavyHitter*) lhs)->lower - ((const HeavyHitter*) rhs)->lower;
}

typedef struct descendant {
	LCLitem_t id;
	int mask;
} Descendant;

//the two-dimensional output
HeavyHitter * output2(int threshold, int * numhitters) {
	HeavyHitter * output; //This will be the heavy hitters to output
	int n = 0; //the number of items in output
	int outputspace = 5, hpspace = 5;
	int i, j, k, l, m, im;
	LCLitem_t newIP;
	int s;
	Descendant * Hp; //the direct descendants
	int numdescendants;

	//std::set< std::pair< LCLitem_t, LCLitem_t > > sex; //debug

	output = (HeavyHitter*) CALLOC(sizeof(HeavyHitter), outputspace); //ensure that it is sufficient
	Hp = (Descendant*) CALLOC(hpspace, sizeof(Descendant));
	
	for (i = 0; i < NUM_COUNTERS; i++) {
		#ifndef UNITARY
		for (j = 1; j <= counters[i]->COUNTERSIZE; j++) {
		#else
		for (j = 0; j < counters[i]->COUNTERSIZE; j++) {
		#endif
			if (counters[i]->COUNTERS[j].item != LCL_NULLITEM) {
				//P("count("); PIP(counters[i]->COUNTERS[j].item >> 32); P(", "); PIP(counters[i]->COUNTERS[j].item); P(")[%d] = [%d, %d]\n", i, counters[i]->COUNTERS[j].COUNT - counters[i]->COUNTERS[j].delta, counters[i]->COUNTERS[j].COUNT);
				//Now we just have to check that the counts are sufficient
				//s = amount that needs to be subtracted from the count
				
				if (counters[i]->COUNTERS[j].COUNT < threshold) continue; // no point continuing

				//Compute Hp
				/*
				//This gets mentally confusing so I compute masks in index space and mask space and do asserts.
				if (i % MAX_DEPTH != 0) {//we can specialise in the first dimension
					for (k = 0; k < 256; k++) {
						Hp[numdescendants].i = i - 1;
						Hp[numdescendants].mask = masks[i] | (((LCLitem_t) 255) << (24 + (i % MAX_DEPTH) * 8));
						Hp[numdescendants].id = (counters[i]->COUNTERS[j].item | (((LCLitem_t) k) << (24 + (i % MAX_DEPTH) * 8)));
						assert(masks[Hp[numdescendants].i] == Hp[numdescendants].mask);
						if (sex.end() != sex.find(std::pair<LCLitem_t, LCLitem_t>(Hp[numdescendants].id, Hp[numdescendants].mask))) //debug
							numdescendants++;
					}
				}
				if (i / MAX_DEPTH != 0) {//we can specialise in the second dimension
					for (k = 0; k < 256; k++) {
						Hp[numdescendants].i = i - MAX_DEPTH;
						Hp[numdescendants].mask = (masks[i] | (((LCLitem_t) 255) << ((i / MAX_DEPTH) * 8 - 8)));
						Hp[numdescendants].id = (counters[i]->COUNTERS[j].item | (((LCLitem_t) k) << ((i / MAX_DEPTH) * 8 - 8)));
						assert(masks[Hp[numdescendants].i] == Hp[numdescendants].mask);
						if (sex.end() != sex.find(std::pair<LCLitem_t, LCLitem_t>(Hp[numdescendants].id, Hp[numdescendants].mask))) //debug
							numdescendants++;
					}
				}
				D("%llx has %d descendants\n", counters[i]->COUNTERS[j].item, numdescendants);
				*/
				numdescendants = 0; // |Hp| = 0
				for (k = 0; k < n; k++) {
					if ((masks[i] & masks[output[k].mask]) == masks[i] && (counters[i]->COUNTERS[j].item & masks[i]) == (output[k].item & masks[i])) {
						//output[k] is a descendant---is it direct?
						//assume it is for now, but check to make sure the assumption was valid for the previous ones
						l = 0;
						for (m = 0; m < numdescendants; m++) {
							Hp[l] = Hp[m];
							if ((masks[i] & masks[Hp[m].mask]) != masks[i] || (Hp[m].id & masks[output[k].mask]) != (output[k].item & masks[output[k].mask])) l++;
						}	
						numdescendants = l;
						Hp[numdescendants].mask = output[k].mask;
						Hp[numdescendants].id = output[k].item;
						numdescendants++;
						while (numdescendants >= hpspace) {hpspace *= 2; Hp = (Descendant*) REALLOC(Hp, sizeof(Descendant)*hpspace);}
					}
				}

				//now compute s
				s = 0;
				for (k = 0; k < numdescendants; k++) {
					s += LCL_PointEstLow(counters[Hp[k].mask], Hp[k].id);
				}
				for (k = 0; k < numdescendants; k++) {
					for (l = k + 1; l < numdescendants; l++) {
						//compute glb of Hp[k] and Hp[l]
						//identify the i values to look at
						//im = min(Hp[k].i % MAX_DEPTH, Hp[l].i % MAX_DEPTH) + MAX_DEPTH * min(Hp[k].i / MAX_DEPTH, Hp[l].i / MAX_DEPTH); //or the masks
						//assert(masks[im] == (Hp[k].mask | Hp[l].mask));
						//iM = max(Hp[k].i % MAX_DEPTH, Hp[l].i % MAX_DEPTH) + MAX_DEPTH * max(Hp[k].i / MAX_DEPTH, Hp[l].i / MAX_DEPTH); //and the masks
						//assert(masks[iM] == (Hp[k].mask & Hp[l].mask));

						if ((Hp[k].id & (masks[Hp[k].mask] & masks[Hp[l].mask])) != (Hp[l].id & (masks[Hp[k].mask] & masks[Hp[l].mask]))) continue; //there is no ip common to the subnets k and l

						newIP = (Hp[k].id | Hp[l].id);
						//compute the right mask
						im = 0; while(masks[im] != (masks[Hp[k].mask] | masks[Hp[l].mask])) im++;
						assert(masks[im] == (masks[Hp[k].mask] | masks[Hp[l].mask]));
						s -= LCL_PointEstUpp(counters[im], newIP);
					}
				}

				//now compare to the threshold
				if (counters[i]->COUNTERS[j].COUNT - s >= threshold) {
					//Add this item to our list of heavy hitters
					while (n >= outputspace) {outputspace *= 2; output = (HeavyHitter *) REALLOC(output, outputspace * sizeof(HeavyHitter));}
					output[n].item = counters[i]->COUNTERS[j].item;
					output[n].mask = i;
					output[n].upper = counters[i]->COUNTERS[j].COUNT;
					output[n].lower = output[n].upper - counters[i]->COUNTERS[j].delta;
					//output[n].s = s; output[n].t = numdescendants; //debug
					//sex.insert( std::pair< LCLitem_t, LCLitem_t >(output[n].item, output[n].mask)); ///debug
					n++;
				}

			}
		}
	}
	
	FREE(Hp);

	//now clean up the output
	output = (HeavyHitter*) REALLOC(output, n * sizeof(HeavyHitter));
	//qsort(output, n, sizeof(HeavyHitter), &cmpHH);

	*numhitters = n;

	return output;
}



