
#ifndef DIM2_H
#define DIM2_H

#if NUM_MASKS == 1089 /* 33x33 kjc */
#define NUM_COUNTERS NUM_MASKS //number of masks
#define MAX_DEPTH 33 //depth of masks lattice */ // not used in hhh2
#else
#define NUM_COUNTERS 25 //number of masks
#define MAX_DEPTH 5 //depth of masks lattice
#endif
#define MAX_DESCENDANTS 512 //maximum number of direct descendents of a given ip pair

#include "prng.h"

//make sure we are passing the right #def around
#ifndef DIMENSION2
#error Invalid dimension
#endif

//still define things as in lossycount.h
#ifdef DIMENSION2
#define LCLitem_t uint64__t
#else
#define LCLitem_t uint32_t
#endif

//The masks associated with the counters
//Note that we must ensure that they are in increasing order of generality
extern LCLitem_t masks[NUM_COUNTERS];

//initialise
void init(double epsilon);

//deinitialise
void deinit();

#ifdef LATTICESEARCH
void update(LCLitem_t * item, int count, int threshold);
#else
#ifndef PARALLEL
//update an input
void update(LCLitem_t item, int count);
#else
void update(LCLitem_t * item, int count);
#endif
#endif

//struct to store a heavy hitter output
typedef struct heavyhitter {
	LCLitem_t item; //The item
	int mask; //The mask id
	int upper, lower; //Upper and lower count bounds
	//int s, t;//debug
} HeavyHitter;

//the two-dimensional output
HeavyHitter * output2(int threshold, int * numhitters);

#if 1 /* kjc */
int masks2plen(LCLitem_t masks, int idx);
#endif
#endif


