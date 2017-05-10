/*
The main function for the two-dimensional HHH program
- Thomas Steinke (tsteinke@seas.harvard.edu) 2010-11
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>	
#include "dim2.h"
#include <sys/timeb.h>

#ifdef PARALLEL
#include <omp.h>
#endif

#include "alloc.h"

#ifndef CLK_PER_SEC
#ifdef CLOCKS_PER_SEC
#define CLK_PER_SEC CLOCKS_PER_SEC
#endif
#endif

//the masks
LCLitem_t masks[NUM_COUNTERS] = {
	//255.255.255.255
	//0-4
	0xFFFFFFFFFFFFFFFFull, //255.255.255.255
	0xFFFFFF00FFFFFFFFull, //255.255.255.0
	0xFFFF0000FFFFFFFFull, //255.255.0.0
	0xFF000000FFFFFFFFull, //255.0.0.0
	0x00000000FFFFFFFFull, //0.0.0.0

	//255.255.255.0
	//5-9
	0xFFFFFFFFFFFFFF00ull, //255.255.255.255
	0xFFFFFF00FFFFFF00ull, //255.255.255.0
	0xFFFF0000FFFFFF00ull, //255.255.0.0
	0xFF000000FFFFFF00ull, //255.0.0.0
	0x00000000FFFFFF00ull, //0.0.0.0

	//255.255.0.0
	//10-14
	0xFFFFFFFFFFFF0000ull, //255.255.255.255
	0xFFFFFF00FFFF0000ull, //255.255.255.0
	0xFFFF0000FFFF0000ull, //255.255.0.0
	0xFF000000FFFF0000ull, //255.0.0.0
	0x00000000FFFF0000ull, //0.0.0.0

	//255.0.0.0
	//15-19
	0xFFFFFFFFFF000000ull, //255.255.255.255
	0xFFFFFF00FF000000ull, //255.255.255.0
	0xFFFF0000FF000000ull, //255.255.0.0
	0xFF000000FF000000ull, //255.0.0.0
	0x00000000FF000000ull, //0.0.0.0

	//0.0.0.0
	//20-24
	0xFFFFFFFF00000000ull, //255.255.255.255
	0xFFFFFF0000000000ull, //255.255.255.0
	0xFFFF000000000000ull, //255.255.0.0
	0xFF00000000000000ull, //255.0.0.0
	0x0000000000000000ull  //0.0.0.0
};


/*int _main() {
		int m; //number of heavy hitters in output
		int counters, threshold, n;
		scanf("%d%d%d", &counters, &threshold, &n);
		HeavyHitter * ans;
		int i;
		unsigned long long wa, xa, ya, za;
		unsigned long long wb, xb, yb, zb;
		unsigned long long w, x, y, z;
		unsigned long long a, b, ip, _ip;

		init((double)1/(double)counters);

		for (i = 0; i < n; i++) {
			scanf("%llu%llu%llu%llu", &w, &x, &y, &z);
			ip = (unsigned long long)256*((unsigned long long)256*((unsigned long long)256*w + x) + y) + z;
			scanf("%llu%llu%llu%llu", &w, &x, &y, &z);
			_ip = (unsigned long long)256*((unsigned long long)256*((unsigned long long)256*w + x) + y) + z;
			update(ip << 32 | _ip , 1);
		}

		ans = output2(threshold, &m);

		deinit();

		for (i = 0; i < m; i++) {
			//output ans[i]

			//break up the ip
			a = ans[i].item >> 32;
			za = a % 256; a /= 256;
			ya = a % 256; a /= 256;
			xa = a % 256; a /= 256;
			wa = a % 256; a /= 256;

			//break up the mask
			b = masks[ans[i].mask] >> 32;
			zb = b % 256; b /= 256;
			yb = b % 256; b /= 256;
			xb = b % 256; b /= 256;
			wb = b % 256; b /= 256;

			//output ip&mask
			if (wb != 0) printf("%llu.", wa); else printf("*.");
			if (xb != 0) printf("%llu.", xa); else printf("*.");
			if (yb != 0) printf("%llu.", ya); else printf("*.");
			if (zb != 0) printf("%llu ", za); else printf("* ");

			//break up the ip
			a = ans[i].item;
			za = a % 256; a /= 256;
			ya = a % 256; a /= 256;
			xa = a % 256; a /= 256;
			wa = a % 256; a /= 256;

			//break up the mask
			b = masks[ans[i].mask];
			zb = b % 256; b /= 256;
			yb = b % 256; b /= 256;
			xb = b % 256; b /= 256;
			wb = b % 256; b /= 256;

			//output ip&mask
			if (wb != 0) printf("%llu.", wa); else printf("*.");
			if (xb != 0) printf("%llu.", xa); else printf("*.");
			if (yb != 0) printf("%llu.", ya); else printf("*.");
			if (zb != 0) printf("%llu", za); else printf("*");

			//output counts
			//printf("	[%d, %d] %d %d\n",  ans[i].lower, ans[i].upper, ans[i].s, ans[i].t); //debug
			printf("	[%d, %d]\n",  ans[i].lower, ans[i].upper); //debug
		}

		free(ans);

	return 0;
}*/

double dblmainmax(double a, double b) {return (a >= b ? a : b);}

int main(int argc, char * argv[]) {
		int m; //number of heavy hitters in output
		//scanf("%d%d%d", &counters, &threshold, &n);
		int counters = 100;
		int threshold = 1000;
		int n = 100000;
		double time, walltime;
		double epsil;
		int memory;
		HeavyHitter * ans;
		int i;
		int w, x, y, z;
		clock_t begint, endt;
		struct timeb begintb, endtb;
		unsigned long long ip, _ip;
		unsigned long long * data;
		FILE * fp = NULL;
		FILE * fsummary = NULL; //summary file	
		if (getenv("SUMMARYFILE") != NULL) fsummary = fopen(getenv("SUMMARYFILE"), "a");

		if (argc > 1) n = atoi(argv[1]);
		if (argc > 2) counters = atoi(argv[2]);
		if (argc > 3) threshold = atoi(argv[3]);
		if (argc > 4) fp = fopen(argv[4], "w");

		if(n/counters >= threshold) {
			printf("Unacceptable parameters: eps*n >= theshold\n");
			return 0;
		}

		

		data = (unsigned long long *) malloc(sizeof(unsigned long long) * n);

		init((double)1/(double)counters);

		for (i = 0; i < n; i++) {
			scanf("%d%d%d%d", &w, &x, &y, &z);
			ip = (unsigned long long)256*((unsigned long long)256*((unsigned long long)256*w + x) + y) + z;
			scanf("%d%d%d%d", &w, &x, &y, &z);
			_ip = (unsigned long long)256*((unsigned long long)256*((unsigned long long)256*w + x) + y) + z;
			data[i] = (ip << 32 | _ip);
		}

		begint = clock();
		ftime(&begintb);
		#ifndef PARALLEL
		for (i = 0; i < n; i++) update(data[i], 1);
		#else
		update(data, n);
		#endif
		endt = clock();
		ftime(&endtb);

		time = ((double)(endt-begint))/CLK_PER_SEC;
		walltime = ((double) (endtb.time-begintb.time))+((double)endtb.millitm-(double)begintb.millitm)/1000;
		memory = maxmemusage();

		free(data);
		
		printf( "%d pairs took %lfs %dB [%d counters] ", n, time, memory, counters);

		ans = output2(threshold, &m);

		printf("%d HHHs [%d threshold]\n", m, threshold);

		deinit();

		if (fp != NULL) {
			//fprintf(fp, "%d\n", m);
			fprintf(fp, "%s %d %d %d %d %lf %d\n", argv[0], n, counters, threshold, m, time, memory);
			for (i = 0; i < m; i++) {
				fprintf(fp, "%d %d.%d.%d.%d %d.%d.%d.%d %d %d\n",
				ans[i].mask,
				(int)((ans[i].item >> 56) & (LCLitem_t)255),
				(int)((ans[i].item >> 48) & (LCLitem_t)255),
				(int)((ans[i].item >> 40) & (LCLitem_t)255),
				(int)((ans[i].item >> 32) & (LCLitem_t)255),
				(int)((ans[i].item >> 24)& (LCLitem_t)255),
				(int)((ans[i].item >> 16) & (LCLitem_t)255),
				(int)((ans[i].item >> 8) & (LCLitem_t)255),
				(int)((ans[i].item >> 0) & (LCLitem_t)255),
				ans[i].lower, ans[i].upper);
			}
			fclose(fp);
		}

		epsil = -1;
		for (i = 0; i < m; i++) {
			epsil = dblmainmax(epsil, ((double)(ans[i].upper-ans[i].lower))/n);
		}

		FREE(ans);

		if (fsummary != NULL) {
			fprintf(fsummary, "check=false algorithm=%-14s nitems=%-10d counters=%-5d threshold=%-9d memory=%-7d outputsize=%-3d time=%lf walltime=%lf epsilon=%lf",
		                           argv[0],        n,           counters,     threshold,     memory,     m,              time, walltime,   epsil);
			if (getenv("OMP_NUM_THREADS") != NULL) {
				fprintf(fsummary, " threads=%s", getenv("OMP_NUM_THREADS"));
			}
			fprintf(fsummary, "\n");
			fclose(fsummary);       
		}

	return 0;
}

