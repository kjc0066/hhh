/*
Implementation of the "Multidimensional Algorithm with Partial Ancestry" from "Finding Hierarchical Heavy Hitters in Streaming Data" by Graham Cormode, Flip Korn, S. Muthukrishnan, and Divesh Srivastava.
- Thomas Steinke (tsteinke@seas.harvard.edu) 2010-10
*/

#ifndef ANCESTRY_H
#define ANCESTRY_H

#include "prng.h"
//copied from lossycount.h
//The type being counted
//This depends on the algorithm
#ifdef DIMENSION2
#define LCLitem_t uint64__t
#else
#define LCLitem_t uint32_t
#endif

#ifndef NUM_MASKS
#define NUM_MASKS 5
#endif
extern LCLitem_t masks[NUM_MASKS];


//Initialise the data structure with a specified epsilon accuracy value
void init(double eps);

//Update the count of an item by a number
void update(LCLitem_t item, int count);

//Heavy hitter info
typedef struct heavyhitter {
	LCLitem_t item; //The item
	int mask; //The mask id
	int upper, lower; //Upper and lower count bounds
	//int s, t;//debug
} HeavyHitter;

//Output the heavy hitters
//don't forget to free them
HeavyHitter * output(int threshold, int* num);

//Clean up
void deinit();

#endif

