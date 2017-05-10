#include "prng.h"
#include "ancestry1.h"
#include <stdlib.h>
#include "alloc.h"

#define D(x...) //fprintf(stderr, x)

//#define NUM_MASKS 4
/*LCLitem_t masks[NUM_MASKS] = {
	0xFFFFFFFFu,
	0xFFFFFF00u,
	0xFFFF0000u,
	0xFF000000u,
	0x00000000u
};*/

typedef struct counter Counter;
struct counter {
	LCLitem_t item; //an ip address
	int g; //lower bound on the count of item that is not counted elsewhere in the table
	int d; //upper bound on the count of item before it was inserted
	//g+d is upper bound on the count of item not accounted for elsewhere
	int m; //upper bound on count of any missing child of item
	Counter *next; //hash table linked list
	int s; //counts passed up
	#ifdef FULL_ANCESTRY
		int children; //number of children
	#endif
};

int max(int a, int b) {return (a >= b ? a : b);}

//constants
#define HASHA_VAL 151261303 //hash parameter
#define HASHB_VAL 6722461
//	int hash = (int) hash31(HASHA_VAL, HASHB_VAL, item) % size;

Counter ** hashtable[NUM_MASKS];
int htsize;
double epsilon; //accuracy; all counts are accurate to within epsilon*N
int N; //number of items seen so far

//Update the count of an item by a number
//by default mask = 0
//m can also be updated, by default 0
void _update(LCLitem_t item, int mask, int mupdate, int supdate, int count, int cupdate) {
	int i, hash = (int) hash31(HASHA_VAL, HASHB_VAL, item) % htsize;
	Counter ** p = &hashtable[mask][hash];
	Counter * q, * ni;
	while (*p != NULL) {
		if ((*p)->item == item) {
			D("updating %d.%d.%d.%d\n", 255&((*p)->item>>24), 255&((*p)->item>>16), 255&((*p)->item>>8), 255&((*p)->item));
			(*p)->g += count;
			(*p)->m = max((*p)->m, mupdate);
			(*p)->s += supdate;
			#ifdef FULL_ANCESTRY
				(*p)->children += cupdate;
			#endif
			return;
		}
		p = &((*p)->next);
	}
	//now *p = NULL so insert
	D("inserting %d.%d.%d.%d\n", 255&(item>>24), 255&(item>>16), 255&(item>>8), 255&(item));
	ni = CALLOC(1, sizeof(Counter));
	#ifdef FULL_ANCESTRY
		//insert parent first
		if (mask+1 < NUM_MASKS) _update(item & masks[mask+1], mask+1, 0, 0, 0, 1);
		ni->children = cupdate;
	#endif
	ni->item = item;
	ni->g = count;
	//ni->d = ancestor(**p).m
	ni->d = (int) (epsilon*N); //value if there is no ancestor
	for (i = mask+1; i < NUM_MASKS; i++) {
		//if item & masks[i] can be found use its m value
		hash = (int) hash31(HASHA_VAL, HASHB_VAL, item & masks[i]) % htsize;
		q = hashtable[i][hash];
		while (q != NULL && q->item != (item & masks[i])) q = q->next;
		//now either q = NULL or q is our item
		if (q != NULL) {
			ni->d = q->m;
			break;
		}
	}
	ni->m = max(ni->d, mupdate);
	ni->next = NULL;
	ni->s = supdate;
	*p = ni; //inserted
}

//Initialise the data structure with a specified epsilon accuracy value
void init(double eps) {
	int i;
	epsilon = eps;
	N = 0;
	htsize = (1 + (int) (1.0 / epsilon)) | 1;// not really sure what to put here
	for (i = 0; i < NUM_MASKS; i++)
		hashtable[i] = CALLOC(htsize, sizeof(Counter*));
	#ifndef DUMB_BCURR
		_update(0, NUM_MASKS-1, 0, 0, 0, 0);
	#endif
}

#ifdef FULL_ANCESTRY
int haschild(Counter * p) {
	return (p->children > 0);
}
#else
int haschild(Counter * p) {return 0;}
#endif

//compress the data structure if necessary
void compress() {
	int i, j;
	Counter ** p, *q;
	for (i = 0; i < NUM_MASKS; i++) {
		for (j = 0; j < htsize; j++) {
			p = &hashtable[i][j]; 
			while (*p != NULL) {
				#ifndef DUMB_BCURR
				if ((*p)->g + (*p)->d < epsilon*N && !haschild(*p) && i != NUM_MASKS-1) {
				#else
				if ((*p)->g + (*p)->d < epsilon*N && !haschild(*p)) {
				#endif
					//delete *p
					D("deleting %d.%d.%d.%d\n", 255&((*p)->item>>24), 255&((*p)->item>>16), 255&((*p)->item>>8), 255&((*p)->item));
					q = *p;
					*p = q->next;
					if (i+1 < NUM_MASKS) _update(q->item & masks[i+1], i+1, q->g + q->d, 0, q->g, -1);
					FREE(q);		
				} else {
					p = &((*p)->next);
				}
			}
		}
	}
}

// the default update that only updates with mask 0
//compress if necessary //NB: don't compress in the other update method because that gets called by compress---infinite recursion is bad
void update(LCLitem_t item, int count) {
	_update(item, 0, 0, 0, count, 0);
	N+=count;
	if (N % ((int) (1.0/epsilon)) == 0) compress(); //only compress sometimes
}

int hhhcmp(const void * lhs, const void * rhs) {
	if (((const HeavyHitter *)lhs)->mask < ((const HeavyHitter *)rhs)->mask) return -1;
	if (((const HeavyHitter *)lhs)->mask > ((const HeavyHitter *)rhs)->mask) return 1;
	if (((const HeavyHitter *)lhs)->item < ((const HeavyHitter *)rhs)->item) return -1;
	if (((const HeavyHitter *)lhs)->item > ((const HeavyHitter *)rhs)->item) return 1;
	return 0;
}

//decondition the counts
void postprocess(int n, HeavyHitter * h) {
	int i, j;
	qsort(h, n, sizeof(HeavyHitter), &hhhcmp);
	for (i = n-2; i >= 0; i--) {
		for (j = i+1; j < n; j++) {
			if (h[j].item == (h[i].item & masks[h[j].mask]) ) {
				h[j].upper += h[i].lower;
				h[j].lower += h[i].lower;
			}
		}
	}
}

/*
//Heavy hitter info
typedef struct heavyhitter {
	LCLitem_t item; //The item
	int mask; //The mask id
	int upper, lower; //Upper and lower count bounds
	//int s, t;//debug
} HeavyHitter;*/

//Output the heavy hitters
//don't forget to FREE them
HeavyHitter * output(int threshold, int * num) {
	int i, j;
	int capacity = 1+N/threshold;
	Counter * p;
	*num = 0;
	HeavyHitter * ans = MALLOC(sizeof(HeavyHitter) * capacity);
	D("output\n");
	for (i = 0; i < NUM_MASKS; i++) for (j = 0; j < htsize; j++) for (p = hashtable[i][j]; p != NULL; p = p->next) p->s = 0;
	for (i = 0; i < NUM_MASKS; i++) {
		for (j = 0; j < htsize; j++) {
			for (p = hashtable[i][j]; p != NULL; p = p->next) {
			D("count %d.%d.%d.%d = g%d d%d s%d/%d\n", 255&((p)->item>>24), 255&((p)->item>>16), 255&((p)->item>>8), 255&((p)->item), p->g, p->d, p->slower, p->supper);
				if (p->g + p->d + p->s >= threshold) { //output hhh
					if (capacity <= *num) {capacity *= 2; ans = REALLOC(ans, sizeof(HeavyHitter)*capacity);}
					ans[*num].item = p->item;
					ans[*num].mask = i;
					ans[*num].upper = p->g + p->d + p->s;
					ans[*num].lower = p->g + p->s;
					(*num)++;
				} else if (i != NUM_MASKS - 1) { //pass up count
					_update(p->item & masks[i+1], i + 1, 0, p->g + p->s, 0, 0);
				}
			}
		}
	}
	compress(); //to get rid of all the garbage
	ans = REALLOC(ans, sizeof(HeavyHitter) * *num);
	postprocess(*num, ans);
	return ans;
}

//Clean up
void deinit() {
	int i, j;
	Counter * p,  *q;
	for (i = 0; i < NUM_MASKS; i++) {
		for (j = 0; j < htsize; j++) {
			p = hashtable[i][j];
			while (p != NULL) {
				q = p->next;
				FREE(p);
				p = q;
			}
		}
		FREE(hashtable[i]);
	}	
}

