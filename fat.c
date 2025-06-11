#include "fat.h"
#include "ds.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define SUPER 0
#define TABLE 2
#define DIR 1

#define SIZE 1024

// the superblock
#define MAGIC_N           0xAC0010DE
typedef struct{
	int magic;
	int number_blocks;
	int n_fat_blocks;
	char empty[BLOCK_SIZE-3*sizeof(int)];
} super;

super sb;

//item
#define MAX_LETTERS 6
#define OK 1
#define NON_OK 0
typedef struct{
	unsigned char used;
	char name[MAX_LETTERS+1];
	unsigned int length;
	unsigned int first;
} dir_item;

#define N_ITEMS (BLOCK_SIZE / sizeof(dir_item))
dir_item dir[N_ITEMS];

// table
#define FREE 0
#define EOFF 1
#define BUSY 2
unsigned int *fat;

int mountState = 0;

int fat_format(){ 
	if(mountState == 1) { 
		printf("Arquivo sistema ja montado. ERRO\n");
		return -1;
	}

	int size = ds_size();
	char buffer[BLOCK_SIZE];

	sb.magic = MAGIC_N;
	sb.number_blocks = size;
	sb.n_fat_blocks = ceil(size / (BLOCK_SIZE / sizeof(int)));

	memset(dir, 0, sizeof(dir));

	if(fat) {
		free(fat);
	}
	fat = malloc(size * sizeof(int));
	if(!fat) {
		printf("Erro de alocacao da FAT\n");
		return -1;
	}
	memset(fat, 0, size * sizeof(int));

	memset(buffer, 0, BLOCK_SIZE);
	memcpy(buffer, &sb, sizeof(super));
	ds_write(SUPER, buffer);

	memset(buffer, 0, BLOCK_SIZE);
	memcpy(buffer, dir, sizeof(dir));
	ds_write(DIR, buffer);

	for(int i = 0; i < sb.n_fat_blocks; i++) {
		memset(buffer, 0, BLOCK_SIZE);
		memcpy(buffer, &fat[i * (BLOCK_SIZE / sizeof(int))], BLOCK_SIZE);
		ds_write(TABLE + i, buffer);
	}

  	return 0;
}

void fat_debug(){
	printf("\ndepurando...\n\n");
	
	char buffer[BLOCK_SIZE];
	int i;

	if(mountState == 0) {
		// Ler superbloco
		ds_read(SUPER, buffer);
		memcpy(&sb, buffer, sizeof(super));


		// Ler diretório
		ds_read(DIR, buffer);
		memcpy(&dir, buffer, BLOCK_SIZE);

		// Alocar FAT em RAM e carregar do disco
		if (!fat) fat = malloc(sb.number_blocks * sizeof(int));
		for(i = 0; i < sb.n_fat_blocks; i++) {
			ds_read(TABLE + i, buffer);
			memcpy(&fat[i * (BLOCK_SIZE / sizeof(int))], buffer, BLOCK_SIZE);
		}
	}
	
	// Exibir superbloco
	printf("superblock:\n");
	if(sb.magic == MAGIC_N) {
		printf("\tmagic is ok\n");
	} else {
		printf("\tmagic is WRONG (0x%x)\n", sb.magic);
		return;
	}
	printf("\t%d blocks\n", sb.number_blocks);
	printf("\t%d blocks fat\n", sb.n_fat_blocks);

	printf("%ld", N_ITEMS);
	// Exibir arquivos
	for(i = 0; i < N_ITEMS; i++) {
		if(dir[i].used) {
			printf("\nFile \"%s\": %ld\n", dir[i].name, strlen(dir[i].name));
			printf("\tsize: %u bytes\n", dir[i].length);
			printf("\tBlocks:");
			unsigned int b = dir[i].first;
			while(b != EOFF && b != FREE) {
				printf(" %u", b);
				b = fat[b];
			}
			printf("\n");
		}
	}

	if(mountState == 0) {
		printf("\nFile system not mounted.\n\n");
		free(fat);
	} else {
		printf("\nFile system mounted.\n\n");
	}
}

int fat_mount(){
  	char buffer[BLOCK_SIZE];

	// Ler superbloco
	ds_read(SUPER, buffer);
	memcpy(&sb, buffer, sizeof(super));

	if (sb.magic != MAGIC_N) {
		printf("superbloco invalido: magic = 0x%x\n", sb.magic);
		return -1;
	}

	// Ler diretório
	ds_read(DIR, buffer);
	memcpy(&dir, buffer, sizeof(dir));

	// Alocar FAT e carregar do disco
	if (mountState == 1) free(fat);
	fat = malloc(sb.number_blocks * sizeof(int));
	if (!fat) {
		printf("erro de alocacao da FAT\n");
		return -1;
	}

	for (int i = 0; i < sb.n_fat_blocks; i++) {
		ds_read(TABLE + i, buffer);
		memcpy(&fat[i * (BLOCK_SIZE / sizeof(int))], buffer, BLOCK_SIZE);
	}

	mountState = 1;
	return 0;
}

int fat_create(char *name){

	if(mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}

	for(int i = 0; i < N_ITEMS; i++) {
		if(dir[i].used && strcmp(dir[i].name, name) == 0) {
			printf("Arquivo ja existe. ERRO\n");
			return -1;
		}
	}

	if(strlen(name) > MAX_LETTERS) {
		printf("Nome do arquivo muito longo. ERRO\n");
		return -1;
	}

	for(int i = 0; i < N_ITEMS; i++) {
		if(!dir[i].used) {
			dir[i].used = 1;
			strcpy(dir[i].name, name);
			dir[i].name[MAX_LETTERS] = '\0';
			dir[i].length = 0;
			dir[i].first = EOFF;
			char buffer[BLOCK_SIZE];
			memset(buffer, 0, BLOCK_SIZE);
			memcpy(buffer, dir, sizeof(dir));
			ds_write(DIR, buffer);
			printf("Arquivo \"%s\" criado com sucesso.\n", name);

			return 0;
		}
	}
	printf("Diretorio cheio. ERRO\n");
	return -1;
}

int fat_delete( char *name){
	//Remove o arquivo, liberando todos os blocos associados com o nome, 
	//atualizando a FAT na RAM e no disco. Em seguida, libera a entrada no diretório. 
	//Devolver 0 indica sucesso e -1, erro.

	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}

	int found = 0;
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used && strcmp(dir[i].name, name) == 0) {
			found = 1;
			unsigned int b = dir[i].first;
			// Libera todos os blocos na FAT
			while (b != EOFF && b != FREE) {
				unsigned int next = fat[b];
				fat[b] = FREE;
				b = next;
			}
			// Limpa a entrada do diretório
			memset(&dir[i], 0, sizeof(dir_item));

			// Atualiza FAT no disco
			char buffer[BLOCK_SIZE];
			for (int j = 0; j < sb.n_fat_blocks; j++) {
				memset(buffer, 0, BLOCK_SIZE);
				memcpy(buffer, &fat[j * (BLOCK_SIZE / sizeof(int))], BLOCK_SIZE);
				ds_write(TABLE + j, buffer);
			}
			// Atualiza diretório no disco
			memset(buffer, 0, BLOCK_SIZE);
			memcpy(buffer, dir, sizeof(dir));
			ds_write(DIR, buffer);

			printf("Arquivo \"%s\" removido com sucesso.\n", name);
			return 0;
		}
	}
	if (!found) {
		printf("Arquivo nao encontrado. ERRO\n");
		return -1;
	}
  	return 0;
}

int fat_getsize( char *name){ 
	//Devolve o número de bytes do arquivo. Em caso de erro, devolve -1.
	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used && strcmp(dir[i].name, name) == 0) {
			return dir[i].length;
		}
	}
	printf("Arquivo nao encontrado. ERRO\n");
	return -1;
}

//Retorna a quantidade de caracteres lidos
int fat_read( char *name, char *buff, int length, int offset){
	//Lê dados de um arquivo válido. Copia length bytes do arquivo para buff, começando
 	// offset bytes a partir do início do arquivo. Devolve o total de bytes lidos. Esse valor
	//pode ser menor que length se chega ao fim do arquivo. Em caso de erro, devolve -1.

	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used && strcmp(dir[i].name, name) == 0) {
				

			unsigned int b = dir[i].first;
			int bytes_read = 0;
			int bytes_to_read = length;

			// Verifica se o arquivo está vazio ou não alocado antes de avançar
			if (b == EOFF || b == FREE) {
				printf("Arquivo vazio ou não alocado. ERRO\n");
				return -1;
			}

			// Avança até o bloco correto
			while (b != EOFF && b != FREE && offset >= BLOCK_SIZE) {
				offset -= BLOCK_SIZE;
				b = fat[b];
			}

			char block[BLOCK_SIZE];
			while (b != EOFF && b != FREE && bytes_to_read > 0 ) {
				if (b + TABLE >= sb.number_blocks) {
					printf("Bloco fora dos limites. ERRO\n");
					return -1;
				}
				ds_read(TABLE + b, block);
				int to_copy = BLOCK_SIZE - offset < bytes_to_read ? BLOCK_SIZE - offset : bytes_to_read;
				memcpy(buff + bytes_read, block + offset, to_copy);
				bytes_read += to_copy;
				bytes_to_read -= to_copy;
				offset = 0; // Após o primeiro bloco, offset é sempre 0
				b = fat[b];
			}

			return bytes_read;
		}
	}
	printf("Arquivo nao encontrado. ERRO\n");
	return -1;
}

//Retorna a quantidade de caracteres escritos
int fat_write( char *name, const char *buff, int length, int offset){
	//Escreve dados em um arquivo. Copia length bytes de buff para o arquivo,
	//começando de offset bytes a partir do início do arquivo. Em geral, essa operação
	//envolve a alocação de blocos livres. Devolve o total de bytes escritos. Esse valor
	//pode ser menor que length, por exemplo, se o disco enche. Em caso de erro,
	//devolve -1.

	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	} 
	
	
	
	



	printf("Arquivo nao encontrado. ERRO\n");	
	return -1;
}
