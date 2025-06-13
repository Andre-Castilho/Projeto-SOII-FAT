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
	/*Cria um novo sistema de arquivos no disco atual, apagando tudo se for o caso. Cria o superbloco, o diretório e a FAT. 
	Se alguém tentar formatar um sistema de arquivos montado, nada deve ser feito, e um código de erro é devolvido. 
	Devolver 0 indica sucesso e -1, erro.*/

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

	// Alocar FAT em RAM
	if(fat) {
		free(fat);
	}
	fat = malloc(size * sizeof(int));
	if(!fat) {
		printf("Erro de alocacao da FAT\n");
		return -1;
	}
	// Inicializar FAT
	memset(fat, 0, size * sizeof(int));

	// Marcar blocos de FAT como livres
	memset(buffer, 0, BLOCK_SIZE);
	memcpy(buffer, &sb, sizeof(super));
	ds_write(SUPER, buffer);

	// Escrever diretório vazio no disco
	memset(buffer, 0, BLOCK_SIZE);
	memcpy(buffer, dir, sizeof(dir));
	ds_write(DIR, buffer);

	// Escrever FAT no disco
	for(int i = 0; i < sb.n_fat_blocks; i++) {
		memset(buffer, 0, BLOCK_SIZE);
		memcpy(buffer, &fat[i * (BLOCK_SIZE / sizeof(int))], BLOCK_SIZE);
		ds_write(TABLE + i, buffer);
	}

  	return 0;
}

void fat_debug(){
	/*Exibe informações do disco. */

	printf("\ndepurando...\n\n");
	
	char buffer[BLOCK_SIZE];
	int i;

	// faz todo o carregamento do disco, apenas se nao estiver montado
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

	// Exibir arquivos
	for(i = 0; i < N_ITEMS; i++) {
		if(dir[i].used) {
			printf("\nFile \"%s\": \n", dir[i].name);
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
	} else {
		printf("\nFile system mounted.\n\n");
	}
}

int fat_mount(){
	/*Verifica se o sistema de arquivos é válido. Se for, traz a FAT e o diretório para a RAM, representadas com registros (structures). 
	As operações abaixo não têm como funcionar se o sistema de arquivos não estiver montado. 
	Devolver 0 indica sucesso e -1, erro.*/

	char buffer[BLOCK_SIZE];

	// Ler superbloco
	ds_read(SUPER, buffer);
	memcpy(&sb, buffer, sizeof(super));

	if (sb.magic != MAGIC_N) {
		printf("superbloco invalido: magic = 0x%x\n", sb.magic);
		return -1;
	}

	if (sb.number_blocks <= 0 || sb.number_blocks > 100000) {
		printf("superbloco invalido: number_blocks = %d\n", sb.number_blocks);
		return -1;
	}

	// Ler diretório
	ds_read(DIR, buffer);
	memcpy(&dir, buffer, sizeof(dir));

	// Alocar FAT e carregar do disco
	if (fat) free(fat);
	fat = malloc(sb.number_blocks * sizeof(int));
	if (!fat) {
		printf("erro de alocacao da FAT\n");
		return -1;
	}

	int entries_per_block = BLOCK_SIZE / sizeof(int);
	for (int i = 0; i < sb.n_fat_blocks; i++) {
		ds_read(TABLE + i, buffer);
		int start = i * entries_per_block;
		int count = entries_per_block;

		if (start + count > sb.number_blocks)
			count = sb.number_blocks - start;

		memcpy(&fat[start], buffer, count * sizeof(int));
	}

	mountState = 1;
	return 0;
}

int fat_create(char *name){
	//Cria uma entrada de diretório descrevendo um arquivo vazio. A atualização do diretório acontece na RAM e no disco. Devolver 0 indica sucesso e -1, erro.

	// Verifica se o sistema de arquivos está montado
	if(mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}

	// Verifica se o nome do arquivo já existe
	for(int i = 0; i < N_ITEMS; i++) {
		if(dir[i].used && strcmp(dir[i].name, name) == 0) {
			printf("Arquivo ja existe. ERRO\n");
			return -1;
		}
	}

	// Verifica se o nome do arquivo é válido
	if(strlen(name) > MAX_LETTERS) {
		printf("Nome do arquivo muito longo. ERRO\n");
		return -1;
	}

	// Verifica se o diretório tem espaço
	for(int i = 0; i < N_ITEMS; i++) {
		// Se encontrar uma entrada não usada, cria o arquivo
		// e atualiza o diretório
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
	/*Remove o arquivo, liberando todos os blocos associados com o nome, 
	atualizando a FAT na RAM e no disco. Em seguida, libera a entrada no diretório. 
	Devolver 0 indica sucesso e -1, erro. */

	//Verifica se o sistema de arquivos está montado
	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}


	//Procura o arquivo no diretório
	int found = 0;
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used && strcmp(dir[i].name, name) == 0) {
			found = 1; //arquivo encontrado
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
	
	//Verifica se o sistema de arquivos está montado
	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	}
	//Procura o arquivo no diretório e retorna o tamanho
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
	/*Lê dados de um arquivo válido. Copia length bytes do arquivo para buff, começando
 	 offset bytes a partir do início do arquivo. Devolve o total de bytes lidos. Esse valor
	pode ser menor que length se chega ao fim do arquivo. Em caso de erro, devolve -1. */

	// Verifica se o sistema de arquivos está montado
    if (mountState == 0) {
        printf("Arquivo sistema nao montado. ERRO\n");
        return -1;
    }
	//Procura o arquivo no diretório
    for (int i = 0; i < N_ITEMS; i++) {
        if (dir[i].used && strcmp(dir[i].name, name) == 0) {
            unsigned int b = dir[i].first;
            int file_size = dir[i].length;

            if (offset >= file_size) return 0; // nada a ler

			//define o quanto ler
            int max_to_read = file_size - offset;
            if (length > max_to_read) length = max_to_read;

            int bytes_read = 0;
            int bytes_to_read = length;

			//se b estiver livre ou for fim do arquivo, retorna erro
            if (b == EOFF || b == FREE) {
                printf("Arquivo vazio ou não alocado. ERRO\n");
                return -1;
            }

			// Navega até o bloco correto
            while (b != EOFF && b != FREE && offset >= BLOCK_SIZE) {
                offset -= BLOCK_SIZE;
                b = fat[b];
            }

			//faz a leitura do arquivo em si
            char block[BLOCK_SIZE];
            while (b != EOFF && b != FREE && bytes_to_read > 0) {
                ds_read(b, block);
                int to_copy = BLOCK_SIZE - offset < bytes_to_read ? BLOCK_SIZE - offset : bytes_to_read;
                memcpy(buff + bytes_read, block + offset, to_copy);
                bytes_read += to_copy;
                bytes_to_read -= to_copy;
                offset = 0;
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
	/*Escreve dados em um arquivo. Copia length bytes de buff para o arquivo,
	começando de offset bytes a partir do início do arquivo. Em geral, essa operação
	envolve a alocação de blocos livres. Devolve o total de bytes escritos. Esse valor
	pode ser menor que length, por exemplo, se o disco enche. Em caso de erro,
	devolve -1.*/

	// Verifica se o sistema de arquivos está montado
	if (mountState == 0) {
		printf("Arquivo sistema nao montado. ERRO\n");
		return -1;
	} 

    // Encontrar o arquivo no diretório
    int file_index = -1;
    for (int i = 0; i < N_ITEMS; i++) {
        if (dir[i].used && strcmp(dir[i].name, name) == 0) {
            file_index = i;
            break;
        }
    }

	// Se o arquivo não foi encontrado, retorna erro
    if (file_index == -1) {
        printf("Arquivo nao encontrado. ERRO\n");
        return -1;
    }

    dir_item *file = &dir[file_index];

    // Se ainda não tem blocos, é preciso alocar o primeiro
    if (file->first == EOFF || file->first == FREE) {
        for (int i = TABLE + sb.n_fat_blocks; i < sb.number_blocks; i++) {
            if (fat[i] == FREE) {
                file->first = i;
                fat[i] = EOFF;
                break;
            }
        }
        if (file->first == EOFF || file->first == FREE) {
            printf("Sem blocos livres\n");
            return -1;
        }
    }

    // Navega até o bloco de início de escrita
    unsigned int b = file->first;
    unsigned int prev = -1;
    int offset_copy = offset;
    while (offset_copy >= BLOCK_SIZE) {
        if (fat[b] == EOFF) {
            // Alocar novo bloco
            int new_block = -1;
            for (int i = TABLE + sb.n_fat_blocks; i < sb.number_blocks; i++) {
                if (fat[i] == FREE) {
                    new_block = i;
                    break;
                }
            }
			// se nao houver blocos livres, retorna erro
            if (new_block == -1) {
                printf("Sem blocos livres\n");
                return -1;
            }
			// Atualiza a FAT
            fat[b] = new_block;
            fat[new_block] = EOFF;
        }
		// Navega para o próximo bloco
        b = fat[b];
        offset_copy -= BLOCK_SIZE;
    }

    int written = 0;
    int remaining = length;
    int internal_offset = offset % BLOCK_SIZE;
    char block[BLOCK_SIZE];

	//faz a escrita no arquivo em si
    while (remaining > 0) {
        ds_read(b, block);

        int to_write = BLOCK_SIZE - internal_offset;
        if (to_write > remaining) to_write = remaining;

        memcpy(block + internal_offset, buff + written, to_write);
        ds_write(b, block);

        written += to_write;
        remaining -= to_write;
        internal_offset = 0;

		// Se ainda há bytes a escrever, navega para o próximo bloco
        if (remaining > 0) {
            if (fat[b] == EOFF) {
                // Alocar novo bloco
                int new_block = -1;
                for (int i = TABLE + sb.n_fat_blocks; i < sb.number_blocks; i++) {
                    if (fat[i] == FREE) {
                        new_block = i;
                        break;
                    }
                }
                if (new_block == -1) {
                    printf("Sem blocos livres\n");
                    break;  // disco cheio
                }
				// Atualiza a FAT
                fat[b] = new_block;
                fat[new_block] = EOFF;
            }
			// Navega para o próximo bloco
            b = fat[b];
        }
    }

	// Atualiza o tamanho do arquivo no diretório
    if (offset + written > file->length)
        file->length = offset + written;

    // Atualiza diretório no disco
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, dir, sizeof(dir));
    ds_write(DIR, buffer);

    // Atualiza FAT no disco
    for (int i = 0; i < sb.n_fat_blocks; i++) {
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &fat[i * (BLOCK_SIZE / sizeof(int))], BLOCK_SIZE);
        ds_write(TABLE + i, buffer);
    }

    return written;
}
