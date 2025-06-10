#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"

static int number_blocks=0;
static int number_reads=0;
static int number_writes=0;
static FILE *disk;

int ds_size()
{
	return number_blocks;
}

int ds_init( const char *filename, int n )
{
	disk = fopen(filename,"r+");
	if(!disk) disk = fopen(filename,"w+");
	if(!disk) return 0;

	ftruncate(fileno(disk),n * BLOCK_SIZE);

	number_blocks = n;
	number_reads = 0;
	number_writes = 0;

	return 1;
}

static void check( int number, const void *buff )
{
	if(number<0) {
		printf("ERROR: blocknum (%d) is negative!\n",number);
		abort();
	}

	if(number>=number_blocks) {
		printf("ERROR: blocknum (%d) is too big!\n",number);
		abort();
	}

	if(!buff) {
		printf("ERROR: null data pointer!\n");
		abort();
	}
}

void ds_read( int number, char *buff )
{
	int x;
	check(number,buff);
	fseek(disk,number*BLOCK_SIZE,SEEK_SET);
	x = fread(buff,BLOCK_SIZE,1,disk);
	if(x==1) {
		number_reads++;
	} else {
		printf("disk simulation failed\n");
		perror("ds");
		exit(1);
	}
}

void ds_write( int number, const char *buff )
{
	int x;
	check(number,buff);
	fseek(disk,number*BLOCK_SIZE,SEEK_SET);
	x = fwrite(buff,BLOCK_SIZE,1,disk);
	if(x==1) {
		number_writes++;
	} else {
		printf("disk simulation failed\n");
		perror("ds");
		exit(1);
	}
}

void ds_close()
{
	printf("%d reads\n",number_reads);
	printf("%d writes\n",number_writes);
	fclose(disk);
}

