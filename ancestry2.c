/*
Two-dimensional partial ancestry algorithm from "Finding Hierarchical Heavy Hitters in Streaming Data" by Graham Cormode, Flip Korn, S. Muthukrishnan, and Divesh Srivastava
- Thomas Steinke (tsteinke@seas.harvard.edu) 2010-11
*/

#include "dim2.h"
#include "alloc.h"

#define D(x...) //fprintf(stderr, x)
#define DIP(item) //fprintf(stderr, "%d.%d.%d.%d", (int)(255&((item) >> 24)), (int)(255&((item) >> 16)), (int)(255&((item) >> 8)), (int)(255&((item) >> 0)))
#define P(x...) fprintf(stderr, x)
#define PIP(item) fprintf(stderr, "%d.%d.%d.%d", (int)(255&((item) >> 24)), (int)(255&((item) >> 16)), (int)(255&((item) >> 8)), (int)(255&((item) >> 0)))

#include <stdlib.h>

//in the full ancestry certain terms can be zeroed out
#ifdef FULL_ANCESTRY
#define ZP(x) 0
#else
#define ZP(x) x
#endif

int min(int a, int b) {return (a <= b ? a : b);}
int max(int a, int b) {return (a >= b ? a : b);}

//the order that the counters need to be traversed in to get increasing number of generalized attributes
/*int levelorder[NUM_COUNTERS] = {
0, //0 
1, 5, //1
2, 6, 10, //2
3, 7, 11, 15, //3
4, 8, 12, 16, 20, //4
9, 13, 17, 21, //5
14, 18, 22, //6
19, 23, //7
24 //8
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


//constants
#define HASHA_VAL 151261303 //hash parameter
#define HASHB_VAL 6722461
//	int hash = (int) hash31(HASHA_VAL, HASHB_VAL, item) % size;

Counter ** hashtable[NUM_COUNTERS];
int htsize;
double epsilon; //accuracy; all counts are accurate to within epsilon*N
int N; //number of items seen so far


//deinitialise
void deinit() {
	int i, j;
	Counter * p,  *q;
	for (i = 0; i < NUM_COUNTERS; i++) {
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


//Compute m --- the upper bound on d for a given vertex
int findm(LCLitem_t item, int mask, int visited[]) {
	int ans = N;//(int) (epsilon * N);
	int hash;
	Counter *p;
	if (visited[mask]) return ans; //don't revisit the same node
	visited[mask] = 1;
	//minimum over all immediate ancestors and epsilon*N
	if (mask % MAX_DEPTH < MAX_DEPTH-1) {//try generalising in this dimension
		hash = (int) hash31(HASHA_VAL, HASHB_VAL, item & masks[mask+1]) % htsize;
		p = hashtable[mask+1][hash];
		while (p != NULL) {
			if (p->item == (item & masks[mask+1])) break;
			p = p->next;
		}
		if (p != NULL && p->m >= 0) {//found immediate ancestor
			ans = min(ans, p->m);
		} else {//keep looking for ancestor
			ans = min(ans, findm(item, mask+1, visited));
		}
	}
	if (mask / MAX_DEPTH < MAX_DEPTH-1) {//try generalising in this dimension
		hash = (int) hash31(HASHA_VAL, HASHB_VAL, item & masks[mask+MAX_DEPTH]) % htsize;
		p = hashtable[mask+MAX_DEPTH][hash];
		while (p != NULL) {
			if (p->item == (item & masks[mask+MAX_DEPTH])) break;
			p = p->next;
		}
		if (p != NULL && p->m >= 0) {//found immediate ancestor
			ans = min(ans, p->m);
		} else {//keep looking for ancestor
			ans = min(ans, findm(item, mask+MAX_DEPTH, visited));
		}
	}
	return ans;
}

//Update the count of an item by a number
//by default mask = 0
//m can also be updated, by default 0
//special updates do not affect m values and have no associated error
//SPECIAL UPDATES MUST BE CLEANED UP BECAUSE THEY WILL F*** UP THE TREE (unless we have full ancestry anyway)
void _update(LCLitem_t item, int mask, int mupdate, int supdate, int count, int cupdate, int special) {
	int i, nvisited, fm, hash = (int) hash31(HASHA_VAL, HASHB_VAL, item) % htsize;
	Counter ** p = &hashtable[mask][hash];
	Counter * ni;
	int visited[NUM_COUNTERS];
	for (i=0;i<NUM_COUNTERS;i++)visited[i]=0;
	//fprintf(stderr, "update %llx %d ", item, mask);
	while (*p != NULL) {
		if ((*p)->item == item) {
			(*p)->g += count;
			if ((*p)->m < 0 && mupdate >= 0) 
				(*p)->m = max(findm(item, mask, visited), mupdate);
			else
				(*p)->m = max((*p)->m, mupdate);
			(*p)->s += supdate;
			#ifdef FULL_ANCESTRY
				(*p)->children += cupdate;
			#endif
			return;
		}
		p = &((*p)->next);
	}
	//P("inserting "); PIP(item >> 32); P(" "); PIP(item); P(" %d\n", mask);
	//now *p = NULL so insert
	#ifdef FULL_ANCESTRY
		//insert parents first
		if (mask % MAX_DEPTH < MAX_DEPTH - 1) _update(item & masks[mask+1], mask+1, -special, 0, 0, 1-special, 0);
		if (mask / MAX_DEPTH < MAX_DEPTH - 1) _update(item & masks[mask+MAX_DEPTH], mask+MAX_DEPTH, -special, 0, 0, 1-special, 0);
	#endif

	ni = CALLOC(1, sizeof(Counter));
	ni->item = item;
	ni->g = count;
	//ni->d = ancestor(**p).m
	fm=findm(item, mask, visited);
	nvisited = 0; for (i = 0; i < NUM_COUNTERS; i++) {nvisited += visited[i]; visited[i]=0;}
	//P("inserting[%d] ", special); PIP(item >> 32); P(" "); PIP(item); P(" %d: fm=%d visited=%d\t", mask, fm, nvisited);
	//D("inserting "); DIP(item >> 32); D(" "); DIP(item); D(" count=%d d=%d\n", count, fm);
	ni->d = fm; //special update
	/*//now iterate over all possible ancestors
	for (i = 0; i < NUM_COUNTERS; i++) //iterate through all parents
	if (???) {//only two parents worth ////////////////////////////////////////HILFE!
		//if item & masks[i] can be found use its m value
		hash = (int) hash31(HASHA_VAL, HASHB_VAL, item & masks[i]) % htsize;
		q = hashtable[i][hash];
		while (q != NULL && q->item != (item & masks[i])) q = q->next;
		//now either q = NULL or q is our item
		if (q != NULL) {//found an ancestor
			ni->d = min(ni->d, q->m);
		}
	}*/
	ni->m = (special == 0 ? max(mupdate, fm) : -1);
	ni->next = NULL;
	ni->s = supdate;
	#ifdef FULL_ANCESTRY
		ni->children = cupdate;
	#endif
	*p = ni; //inserted
	//fprintf(stderr, "m=%d\n", ni->m);
	//P("fm(36.7.88.123 36.6.215.*)=%d\n", findm(((LCLitem_t)(((36*256+7)*256+88)*256+123))<<32|((LCLitem_t)(((36*256+6)*256+215)*256)),5,visited));
}

//Initialise the data structure with a specified epsilon accuracy value
void init(double eps) {
	int i;
	#ifdef FULL_ANCESTRY
		epsilon = eps;
	#else
		epsilon = eps/2;
	#endif
	N = 0;
	htsize = (1 + (int) (1.0 / epsilon)) | 1;// not really sure what to put here
	for (i = 0; i < NUM_COUNTERS; i++)
		hashtable[i] = CALLOC(htsize, sizeof(Counter*));
	//P("MAX_DEPTH=%d\n", MAX_DEPTH);
	#ifndef DUMB_BCURR
		_update(0,NUM_COUNTERS-1,0,0,0,0,0);
	#endif
}

#ifdef FULL_ANCESTRY
int haschild(Counter * p) {
	return (p->children > 0);
}
#else
int haschild(Counter * p) {return 0;}//don't worry about children
#endif

//compress the data structure if necessary
void compress() {
	int i, j;
	Counter ** p, *q;
	//D("compress called\n");
	for (i = 0; i < NUM_COUNTERS-1; i++) {
		for (j = 0; j < htsize; j++) {
			p = &hashtable[i][j]; 
			while (*p != NULL) {
				if (abs((*p)->g) + abs((*p)->d) < epsilon*N && !haschild(*p)) {
					//delete *p
					q = *p;
					*p = q->next;
					//D("deleting "); DIP(q->item >> 32); D(" "); DIP(q->item); D(" %d %d\n", q->g, q->g + q->d);
					//pass stuff up to les ancestorez
					//1-d version: if (i+1 < NUM_COUNTERS) _update(q->item & masks[i+1], i+1, q->g + q->d, 0, 0, q->g);
					if (i + 1 == NUM_COUNTERS) {//do nothing
					} else if ((i % MAX_DEPTH) == MAX_DEPTH-1) {//unique parent
						_update(q->item & masks[i+MAX_DEPTH], i+MAX_DEPTH, max(q->m, abs(q->g) + abs(q->d)), 0, q->g, -1, 0);
					} else if ((i / MAX_DEPTH) == MAX_DEPTH-1) {//unique parent
						_update(q->item & masks[i+1], i+1, max(q->m, abs(q->g) + abs(q->d)), 0, q->g, -1, 0);
					} else {//multiple parent case
						_update(q->item & masks[i+MAX_DEPTH], i+MAX_DEPTH, max(q->m, abs(q->g) + abs(q->d)), 0, q->g, -1, 0);
						_update(q->item & masks[i+1], i+1, max(q->m, abs(q->g) + abs(q->d)), 0, q->g, -1, 0);
						_update(q->item & masks[i+MAX_DEPTH+1], i+MAX_DEPTH+1, -1, 0, -q->g, 0, 0); //subtract for double counting
					}
					FREE(q);		
				} else {
					p = &((*p)->next);
				}
			}
		}
	}
	//D("compress done\n");
}

// the default update that only updates with mask 0
//compress if necessary //NB: don't compress in the other update method because that gets called by compress---infinite recursion is bad
void update(LCLitem_t item, int count) {
	//int i, j, degdiff=0; Counter * p;
	D("inserting "); DIP(item >> 32); D(" "); DIP(item); D("\n");
	_update(item, 0, 0, 0, count, 0, 0);
	N+=count;
	if (N % ((int) (1.0/epsilon)) == 0) {compress(); D("compressing\n");}//only compress sometimes
	/*#ifdef FULL_ANCESTRY
	for (i = 0; i < NUM_COUNTERS; i++) {
		for (j = 0; j < htsize; j++) {
			for (p = hashtable[levelorder[i]][j]; p != NULL; p = p->next) {
				P("%d ", levelorder[i]); PIP(p->item >> 32); P(" "); PIP(p->item); P(" %d\n", p->children); 
				degdiff -= p->children; //- indegree
				//+ outdegree
				if (levelorder[i] % MAX_DEPTH < MAX_DEPTH-1) degdiff++;
				if (levelorder[i] / MAX_DEPTH < MAX_DEPTH-1) degdiff++; 
				assert(p->children >= 0);
			}
		}
	}
	printf("degdiff=%d\n", degdiff);
	#endif*/
}

/*/struct to store a heavy hitter output
typedef struct heavyhitter {
	LCLitem_t item; //The item
	int mask; //The mask id
	int upper, lower; //Upper and lower count bounds
	//int s, t;//debug
} HeavyHitter;*/

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
			if ( (h[j].mask % MAX_DEPTH) >= (h[i].mask % MAX_DEPTH) && (h[j].mask / MAX_DEPTH) >= (h[i].mask / MAX_DEPTH) &&
				h[j].item == (h[i].item & masks[h[j].mask]) ) {
				h[j].upper += h[i].upper;
				h[j].lower += h[i].lower;
			}
		}
	}
}

//get upper bound, requires complete tree....
int PointEstUpp(LCLitem_t item, int mask) {
	int visited[NUM_COUNTERS];
	int i, hash = (int) hash31(HASHA_VAL, HASHB_VAL, item) % htsize;
	Counter * p = hashtable[mask][hash];
	while (p != NULL) {
		if (p->item == item) return p->g + p->d + p->s;
		p = p->next;
	}
	for (i=0;i<NUM_COUNTERS;i++)visited[i]=0;
	return findm(item, mask, visited);
}
//get lower bound, requires complete tree....
int PointEstLow(LCLitem_t item, int mask) {
	int hash = (int) hash31(HASHA_VAL, HASHB_VAL, item) % htsize;
	Counter * p = hashtable[mask][hash];
	while (p != NULL) {
		if (p->item == item) return max(0, p->g + p->s - ZP(p->d));
		p = p->next;
	}
	return 0;
}

typedef struct descendant {
	LCLitem_t id;
	int mask;
} Descendant;
//compute a lower bound on the number of duplicaate counts for item,mask
int computedoublecount(HeavyHitter * output, int n, LCLitem_t item, int mask) {
	int k, l, m, im;
	LCLitem_t newIP;
	int s;
	Descendant * Hp; //the direct descendants
	int numdescendants;
	Hp = MALLOC(sizeof(Descendant)*n);
	numdescendants = 0; // |Hp| = 0
	for (k = 0; k < n; k++) {
		if (((masks[mask] & masks[output[k].mask]) == masks[mask]) && (item & masks[mask]) == (output[k].item & masks[mask])) {
			//output[k] is a descendant---is it direct?
			//assume it is for now, but check to make sure the assumption was valid for the previous ones
			l = 0;
			for (m = 0; m < numdescendants; m++) {
				Hp[l] = Hp[m];
				if ((masks[output[k].mask] & masks[Hp[m].mask]) != masks[output[k].mask] || (Hp[m].id & masks[output[k].mask]) != (output[k].item & masks[output[k].mask])) l++;
			}	
			numdescendants = l;
			Hp[numdescendants].mask = output[k].mask;
			Hp[numdescendants].id = output[k].item;
			numdescendants++;
		}
	}

	//now compute s
	s = 0;
	for (k = 0; k < numdescendants; k++) {
		s += PointEstLow(Hp[k].id, Hp[k].mask);
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
			//assert(masks[im] == (masks[Hp[k].mask] | masks[Hp[l].mask]));
			s -= PointEstUpp(newIP, im);
		}
	}
	FREE(Hp);
	D("doublecount %d.%d.%d.%d %d.%d.%d.%d = %d\n", (int)(255&(item>>56)), (int)(255&(item>>48)), (int)(255&(item>>40)), (int)(255&(item>>32)), (int)(255&(item>>24)), (int)(255&(item>>16)), (int)(255&(item>>8)), (int)(255&(item>>0)), s);
	return s;
}

//output HHHs
//don't forget to FREE them
HeavyHitter * output2(int threshold, int * num) {
	int j, k, s;
	int capacity = 1+N/threshold;
	Counter ** p, *pp;
	HeavyHitter * ans = MALLOC(sizeof(HeavyHitter) * capacity);
	*num = 0;
	for (k = 0; k < NUM_COUNTERS; k++) {
		for (j = 0; j < htsize; j++) {
			for (p = &(hashtable[k][j]); *p != NULL;  p = &((*p)->next)) {
				D("considering %d.%d.%d.%d %d.%d.%d.%d %d %d\n", (int)(255&((*p)->item>>56)), (int)(255&((*p)->item>>48)), (int)(255&((*p)->item>>40)), (int)(255&((*p)->item>>32)), (int)(255&((*p)->item>>24)), (int)(255&((*p)->item>>16)), (int)(255&((*p)->item>>8)), (int)(255&((*p)->item>>0)), (*p)->g + (*p)->s, (*p)->g + (*p)->s + (*p)->d);
				if ((*p)->g + (*p)->d +(*p)->s >= threshold) {//candidate hhh
					//compute doublecount
					s = computedoublecount(ans, *num, (*p)->item, k);
					if ((*p)->g + (*p)->d +(*p)->s - s >= threshold) {//output
						D("output %d.%d.%d.%d %d.%d.%d.%d\n", (int)(255&((*p)->item>>56)), (int)(255&((*p)->item>>48)), (int)(255&((*p)->item>>40)), (int)(255&((*p)->item>>32)), (int)(255&((*p)->item>>24)), (int)(255&((*p)->item>>16)), (int)(255&((*p)->item>>8)), (int)(255&((*p)->item>>0)));
						if (capacity <= *num) {capacity *= 2; ans = REALLOC(ans, sizeof(HeavyHitter)*capacity);}
						ans[*num].item = (*p)->item;
						ans[*num].mask = k;
						ans[*num].upper = (*p)->g + (*p)->d +(*p)->s;
						ans[*num].lower = max(0, (*p)->g - ZP((*p)->d) + (*p)->s);
						(*num)++;
					}
				} 
				//pass up counts
				if (k + 1 == NUM_COUNTERS) {//do nothing
				} else if ((k % MAX_DEPTH) == MAX_DEPTH-1) {//unique parent
					_update((*p)->item & masks[k+MAX_DEPTH], k+MAX_DEPTH, -1, (*p)->g + (*p)->s, 0, 0, 1);
				} else if ((k / MAX_DEPTH) == MAX_DEPTH-1) {//unique parent
					_update((*p)->item & masks[k+1], k+1, -1, (*p)->g + (*p)->s,  0, 0, 1);
				} else {//multiple parent case
					_update((*p)->item & masks[k+MAX_DEPTH], k+MAX_DEPTH, -1, (*p)->g + (*p)->s, 0, 0, 1);
					_update((*p)->item & masks[k+1], k+1, -1, (*p)->g + (*p)->s, 0, 0, 1);
					_update((*p)->item & masks[k+MAX_DEPTH+1], k+MAX_DEPTH+1, -1, -((*p)->g + (*p)->s), 0, 0, 1); //subtract for double counting
				}
			}
		}
	}
	//the all-important clean-up
	for (k = 0; k < NUM_COUNTERS; k++) {
		for (j = 0; j < htsize; j++) {
			p = &(hashtable[k][j]); 
			while (*p != NULL) {
				//the all-important clean-up
				if ((*p)->m < 0) {//delete *p
					pp = *p;
					*p = pp->next;
					FREE(pp);	
				} else {
					 p = &((*p)->next);
				}
			}
		}
	}
	ans = REALLOC(ans, *num * sizeof(HeavyHitter));
	//postprocess(*num, ans);
	return ans; //     >:-(
}

