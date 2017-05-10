/*
 * Copyright (C) 2012-2017 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * recursive lattice search algorithm by kjc@iijlab.net
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "dim2.h"
#include "alloc.h"

#include <err.h>
#include <sys/queue.h>

/* odflow_hash is used for odflow accounting */
struct odf_tailq {
	TAILQ_HEAD(odfqh, odflow) odfq_head;
	int nrecord;	/* number of record */
};

struct odflow_hash {
	struct odf_tailq *tbl;
	uint64_t count;
	int nrecord;	/* number of records */
	int nbuckets;	/* number of buckets for tbl */
};

struct odflow {
	uint64_t item;
	uint64_t count;
	int *idx_list;	/* keeps odflow indices during aggregation */
	int idx_size;	/* current data size in use */
	int idx_max;	/* current allocation */

	TAILQ_ENTRY(odflow) odf_chain;  /* for hash table */
};

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 *
 * http://www.burtleburtle.net/bob/hash/spooky.html
 */
#define mix(a, b, c)                                                    \
do {                                                                    \
	a -= b; a -= c; a ^= (c >> 13);                                 \
	b -= c; b -= a; b ^= (a << 8);                                  \
	c -= a; c -= b; c ^= (b >> 13);                                 \
	a -= b; a -= c; a ^= (c >> 12);                                 \
	b -= c; b -= a; b ^= (a << 16);                                 \
	c -= a; c -= b; c ^= (b >> 5);                                  \
	a -= b; a -= c; a ^= (c >> 3);                                  \
	b -= c; b -= a; b ^= (a << 10);                                 \
	c -= a; c -= b; c ^= (b >> 15);                                 \
} while (/*CONSTCOND*/0)

/* 4 positions for sub-areas in the recursive lattice search */
#define POS_LOWER	0
#define POS_LEFT	1
#define POS_RIGHT	2
#define POS_UPPER	3

uint64__t *flow_list = NULL;
int flow_list_size = 0;
int thresh;
static struct odflow_hash *dummy_hash;  /* used in lattice_search for
					 * dummy iteration */

static struct heavyhitter *hhh = NULL;
static int numhhh = 0;

static inline uint32_t slot_fetch(uint64_t v, int n);
struct odflow_hash *odhash_alloc(int n);
void odhash_reset(struct odflow_hash *odfh);
void odhash_free(struct odflow_hash *odfh);
struct odflow *odflow_alloc(uint64_t item);
void odflow_free(struct odflow *odfp);
struct odflow *odflow_lookup(struct odflow_hash *odfh, uint64_t item);
static int odflow_extract(struct odflow_hash *odfh, struct odflow *parent, int pl0, int pl1);
static int odflow_aggregate(struct odflow_hash *odfh, struct odflow *parent, int label[]);
int lattice_search(struct odflow *parent, int pl0, int pl1, int size, int pos);

static inline uint32_t
slot_fetch(uint64_t v, int n)
{
        uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = 0;
        uint8_t *p; 
    
        p = (uint8_t *)&v;
        b += p[3];
        b += p[2] << 24; 
        b += p[1] << 16; 
        b += p[0] << 8;
    
        a += p[7];
        a += p[6] << 24; 
        a += p[5] << 16; 
        a += p[4] << 8;

        mix(a, b, c); 

        return (c & (n - 1));  /* n must be power of 2 */
}

/*
 * allocate odflow hash with the given number of buckets, n.
 * n is rounded up to the next power of 2. limit the max to 4096.
 */
struct odflow_hash *
odhash_alloc(int n)
{
	struct odflow_hash *odfh;
	int i, buckets;

	if ((odfh = CALLOC(1, sizeof(struct odflow_hash))) == NULL)
		err(1, "odhash_alloc: calloc");

	/* round up n to the next power of 2 */
	buckets = 1;
	while (buckets < n) {
		buckets *= 2;
		if (buckets == 1024*16)
			break;	/* max size */
	}

	/* allocate a hash table */
	if ((odfh->tbl = CALLOC(buckets, sizeof(struct odf_tailq))) == NULL)
		err(1, "odhash_alloc: calloc");
	odfh->nbuckets = buckets;
	for (i = 0; i < buckets; i++) {
		TAILQ_INIT(&odfh->tbl[i].odfq_head);
		odfh->tbl[i].nrecord = 0;
	}

	/* initialize counters */
	odfh->count = 0;
	odfh->nrecord = 0;
	return (odfh);
}

/* odhash_reset re-initialize the given odhash */
void
odhash_reset(struct odflow_hash *odfh)
{
	int i;
	struct odflow *odfp;

	if (odfh->nrecord == 0)
		return;
        for (i = 0; i < odfh->nbuckets; i++) {
                while ((odfp = TAILQ_FIRST(&odfh->tbl[i].odfq_head)) != NULL) {
			TAILQ_REMOVE(&odfh->tbl[i].odfq_head, odfp, odf_chain);
			odfh->tbl[i].nrecord--;
			odflow_free(odfp);
		}
	}
	/* initialize counters */
	odfh->count = 0;
	odfh->nrecord = 0;
}

void
odhash_free(struct odflow_hash *odfh)
{
	if (odfh->nrecord > 0)
		odhash_reset(odfh);
	FREE(odfh->tbl);
	FREE(odfh);
}

struct odflow *
odflow_alloc(uint64_t item)
{
	struct odflow *odfp;

	if ((odfp = CALLOC(1, sizeof(struct odflow))) == NULL)
		err(1, "cannot allocate entry cache");

	odfp->item = item;

	return (odfp);
}

void
odflow_free(struct odflow *odfp)
{
	if (odfp->idx_list != NULL)
		FREE(odfp->idx_list);
	free(odfp);
}

/*
 * look up odflow matching the given spec in the hash.
 * if not found, allocate one.
 */
struct odflow *
odflow_lookup(struct odflow_hash *odfh, uint64_t item)
{
        struct odflow *odfp;
	int slot;

	/* get slot id */
	slot = slot_fetch(item, odfh->nbuckets);

	/* find entry */
        TAILQ_FOREACH(odfp, &(odfh->tbl[slot].odfq_head), odf_chain) {
                if (odfp->item == item)
                        break;
	}

	if (odfp == NULL) {
		odfp = odflow_alloc(item);
		odfh->nrecord++;
		TAILQ_INSERT_HEAD(&odfh->tbl[slot].odfq_head, odfp, odf_chain);
		odfh->tbl[slot].nrecord++;
	}
	return (odfp);
}

void init(double epsilon)
{
}

void deinit(void)
{
}

void update(LCLitem_t *item, int count, int threshold)
{
	struct odflow *root;
	int n, i, hhh_count = 0;

	flow_list = item;
	flow_list_size = count;
	thresh = threshold;

	n = (count + threshold - 1) / threshold; // estimate max # of hhh
	n += 1;  // reserve one for [0,0]
	hhh = CALLOC(n, sizeof(struct heavyhitter));
	
	root = odflow_alloc(0);
	root->idx_list = CALLOC(count, sizeof(int));
	if (root->idx_list == NULL)
		err(1, "calloc: root's idx_list");
	root->idx_max = root->idx_size = count;
	for (i = 0; i < count; i++)
		root->idx_list[i] = i;

	/* create a dummy hash containing one dummy entry */
	if (dummy_hash == NULL) {
		dummy_hash = odhash_alloc(1);
		if (dummy_hash == NULL)
			err(1, "odhash_alloc failed!");
		(void)odflow_lookup(dummy_hash, 0);
	}

	/* left bottom edge */
	hhh_count += lattice_search(root, 32, 0, 32, POS_RIGHT);
	/* right bottom edge */
	hhh_count += lattice_search(root, 0, 32, 32, POS_LEFT);
	/* sub-areas */
	hhh_count += lattice_search(root, 0, 0, 32, POS_LOWER);

	FREE(root->idx_list);
	root->idx_list = NULL;
	odflow_free(root);
}

HeavyHitter *
output2(int threshold, int * numhitters)
{
	*numhitters = numhhh;
	return (hhh);
}

/*
 * extract odflows from odflow_hash to tailq.
 * returns the number of flows extracted.
 */
static int
odflow_extract(struct odflow_hash *odfh, struct odflow *parent, int pl0, int pl1)
{
	int i, j, size, nflows = 0;
	struct odflow *odfp;

	/* walk through the odflow_hash */
	for (i = 0; i < odfh->nbuckets; i++) {
                while ((odfp = TAILQ_FIRST(&odfh->tbl[i].odfq_head)) != NULL) {
                        TAILQ_REMOVE(&odfh->tbl[i].odfq_head, odfp, odf_chain);
			odfh->tbl[i].nrecord--;
			if (odfp->count < thresh && (pl0 != 0 || pl1 != 0)) {
				/* under the threshold, discard this entry */
				odflow_free(odfp);
				continue;
			}
			// record this as hhh
			hhh[numhhh].item = odfp->item;
			hhh[numhhh].mask = (pl0 << 16) + pl1; // XXX
			hhh[numhhh].upper = hhh[numhhh].lower  = odfp->count;
			numhhh++;
#if 0
			fprintf(stderr, "#extract: ");
			fprintf(stderr, "%d.%d.%d.%d/%d %d.%d.%d.%d/%d %ld\n",
				(int)((odfp->item >> 56) & (LCLitem_t)255),
				(int)((odfp->item >> 48) & (LCLitem_t)255),
				(int)((odfp->item >> 40) & (LCLitem_t)255),
				(int)((odfp->item >> 32) & (LCLitem_t)255),
				pl0,
				(int)((odfp->item >> 24)& (LCLitem_t)255),
				(int)((odfp->item >> 16) & (LCLitem_t)255),
				(int)((odfp->item >> 8) & (LCLitem_t)255),
				(int)((odfp->item >> 0) & (LCLitem_t)255),
				pl1,
				odfp->count);
#endif

			/* book keeping extracted counts */
			parent->count -= odfp->count;

			nflows++;
				
			/* remove prcoessed odflows from the list */
			size = odfp->idx_size;
			for (j = 0; j < size; j++) {
				int idx = odfp->idx_list[j];
				if (flow_list[idx] != 0) {
					flow_list[idx] = 0;
				}
			}
			FREE(odfp->idx_list);
			odfp->idx_list = NULL;
			odfp->idx_max = odfp->idx_size = 0;
			FREE(odfp);
		} /* while */
	} /* for */
	return (nflows);
}

/*
 * try to aggregate odflows in the list for the given label.
 * new entries matching for the label are created on the hash.
 * returns the number of original flows aggregated.
 */
#define LABEL2MASK(i, j) ((((uint64_t)0xffffffff00000000 >> i) & 0xffffffff) << 32 | (((uint64_t)0xffffffff00000000 >> j) & 0xffffffff))

static int
odflow_aggregate(struct odflow_hash *odfh, struct odflow *parent, int label[])
{
	int i, n = 0;
	struct odflow *odfp;
	uint64_t item, mask;

	/* walk through the odflow cache_list of parent */
	for (i = 0; i < parent->idx_size; i++) {
		int index = parent->idx_list[i];
	
		if ((item = flow_list[index]) == 0)
			continue;  /* removed subentry */

		/* make a new flow_spec with the corresponding label value */
		mask = LABEL2MASK(label[0], label[1]);
#if 0
		fprintf(stderr, "odflow_aggregate:item:0x%lx mask:0x%lx [%d,%d]\n", 
			item, mask, label[0], label[1]);
#endif		
		item &= mask;

		/* insert the new odflow to a temporal hash */
		odfp = odflow_lookup(odfh, item);
		odfp->count += 1;

		/* save this index in idx_list */
		if (odfp->idx_size == odfp->idx_max) {
			/* if full, double the size */
			int newsize;
			if (odfp->idx_max == 0)
				newsize = 64; // initial size
			else
				newsize = odfp->idx_max * 2;
			odfp->idx_list = REALLOC(odfp->idx_list, sizeof(int) * newsize);
			if (odfp->idx_list == NULL)
				err(1, "realloc: idx_list");
			odfp->idx_max = newsize;
		}
		odfp->idx_list[odfp->idx_size] = index;
		odfp->idx_size++;

		n++;
	}
#if 0
	fprintf(stderr, "odflow_aggregate:%d nrecord:%d\n", n, odfh->nrecord);
#endif	
	return (n);
}


#define ON_LEFTEDGE	1
#define ON_RIGHTEDGE	2

static int maxsize = 32; /* for IPv4 */
#if NUM_MASKS == 1089
static int minsize = 1;  /* resolution of prefix length */
#else
static int minsize = 8;  /* resolution of prefix length */
#endif

int
lattice_search(struct odflow *parent, int pl0, int pl1, int size, int pos)
{
	int nflows = 0;	/* how many odflows extracted */
	struct odflow_hash *my_hash = NULL;
	int on_edge = 0;
	int do_aggregate = 1, do_recurse = 1, next2bottom = 0;
	
	/* check if this is on the bottom edge */
	if (pl0 == maxsize)
		on_edge = ON_LEFTEDGE;
	else if (pl1 == maxsize)
		on_edge = ON_RIGHTEDGE;

	if (size <= minsize) {
		do_recurse = 0;
		if (on_edge == ON_LEFTEDGE) {
			/* need to visit the very bottom */
			if (size == minsize && pl1 == maxsize - minsize) {
				do_recurse = 1; next2bottom = 1;
			}
		}
	}
	/* don't extract for upper area, to be done later at an higher level */
	if (pos == POS_UPPER)
		do_aggregate = 0;
	if (!do_aggregate && !do_recurse)
		return 0;

	if (do_aggregate) {
		int n, label[2] = {pl0, pl1};

		/* create new odflows in the hash by the given label pair */
		n = parent->idx_size / 8;  /* estimate hash size */
		my_hash = odhash_alloc(n);
		n = odflow_aggregate(my_hash, parent, label);
		if (n == 0) {
			/* no aggregate flow was created */
			odhash_free(my_hash);
			return 0;
		}
	} else {
		my_hash = dummy_hash;  /* used just for iteration */
	}

	if (do_recurse) {
		int i, delta, subsize;

		delta = subsize = size / 2;
		if (next2bottom) { /* cisiting the very bottom */
			delta = size; subsize = 0;
		}

		for (i = 0; i < my_hash->nbuckets; i++) {
			struct odflow *odfp;

			TAILQ_FOREACH(odfp, &(my_hash->tbl[i].odfq_head), odf_chain) {
				/* recursively visit sub-areas */
				int n, subpos, subpl0, subpl1;
				uint64_t count;

				if (!do_aggregate) /* dummy iteration */
					odfp = parent; /* use parent's */
				for (subpos = 0; subpos < 4; subpos++) {
					if ((subpos == POS_LOWER && on_edge) ||
					    (subpos == POS_LEFT  && on_edge == ON_LEFTEDGE) ||
					    (subpos == POS_RIGHT && on_edge == ON_RIGHTEDGE))
						/* if on the edge, skip bottom and left|right */
						continue;
					if (odfp->count < thresh)
						break; /* residual < thresh */
					/* adjust prefixlen pair for sub-area */
					subpl0 = pl0; subpl1 = pl1;
					if (subpos == POS_LOWER || subpos == POS_LEFT)
						subpl0 += delta;
					if (subpos == POS_LOWER || subpos == POS_RIGHT)
						subpl1 += delta;
					count = odfp->count; /* save count */
					n = lattice_search(odfp, subpl0, subpl1, subsize, subpos);
					nflows += n;
					if (n > 0 && do_aggregate)
						/* propagate extracted count to parent */
						parent->count -= count - odfp->count;
				}
				if (!do_aggregate) /* XXX for dummy_hash */
					break; /* out of TAILQ_FOREACH */
			}
		} /* for */
	} /* do_recurse */

	/* aggregate remaining hhh */
	if (do_aggregate) {
#if 0		
		fprintf(stderr, "[%d,%d] size:%d pos:%d parent's count:%ld\n",
			pl0, pl1, size, pos, parent->count);
#endif
		if (parent->count >= thresh || (pl0 == 0 && pl1 == 0))
			nflows += odflow_extract(my_hash, parent, pl0, pl1);
		odhash_free(my_hash);
	}
	return (nflows);
}
