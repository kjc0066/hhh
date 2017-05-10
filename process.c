#include <stdio.h>
	
/*
tcpdump -q -n -t -r equinix-chicago.dirA.20100121-051900.UTC.anon.pcap | process > 2.dump
*/

char buff[1000000];

int main() {
	int src[4], des[4], r, s, count=0;
	while (!feof(stdin)) {
		gets(buff);
		r=sscanf(buff, "IP %d.%d.%d.%d.%*d > %d.%d.%d.%d.%*d: ",
		&src[0], &src[1], &src[2], &src[3], &des[0], &des[1], &des[2], &des[3]);
		if (r != 8) {
			s=sscanf(buff, "IP %d.%d.%d.%d > %d.%d.%d.%d: ",
			&src[0], &src[1], &src[2], &src[3], &des[0], &des[1], &des[2], &des[3]);
			if (s != 8) {continue;}
		}
		printf("%d %d %d %d \t%d %d %d %d\n", src[0], src[1], src[2], src[3], des[0], des[1], des[2], des[3]);
		count++;
	}
	fprintf(stderr, "done %d\n", count);
	return 0;
}

