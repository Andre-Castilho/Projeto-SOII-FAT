#include "ds.h"
#include "fat.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cpout( char * os_path,  char *name );
int cpin( char *name, char * os_path);

int main( int argc, char *argv[] )
{
	char line[1024];
	char cmd[1024];
	char arg1[1024];
	char arg2[1024];
	int result, args;

	if(argc!=3) {
		printf("uso: %s <arquivo> <quantosblocos>\n",argv[0]);
		return 1;
	}

	if(!ds_init(argv[1],atoi(argv[2]))) {
		printf("falha %s: %s\n",argv[1],strerror(errno));
		return 1;
	}

	printf("simulacao de disco %s com %d blocos\n",argv[1],ds_size());

	while(1) {
		printf(" sys> ");
		fflush(stdout);

		if(!fgets(line,sizeof(line),stdin)) break;

		if(line[0]=='\n') continue;
		line[strlen(line)-1] = 0;

		args = sscanf(line,"%s %s %s",cmd,arg1,arg2);
		if(args==0) continue;

		if(!strcmp(cmd,"formatar")) {
			if(args==1) {
				if(!fat_format()) {
					printf("formatou\n");
				} else {
					printf("falhou na formatacao!\n");
				}
			} else {
				printf("uso: formatar\n");
			}
		} else if(!(strcmp(cmd,"montar"))) {
			if(args==1) {
				if(!fat_mount()) {
					printf("montagem ok\n");
				} else {
					printf("falha de montagem!\n");
				}
			} else {
				printf("uso: montar\n");
			}
		} else if(!strcmp(cmd,"depurar")) {
			if(args==1) {
				fat_debug();
			} else {
				printf("uso: depurar\n");
			}
		} else if(!strcmp(cmd,"medir")) {
			if(args==2) {
				result = fat_getsize(arg1);
				if(result>=0) {
					printf("o arquivo %s mede %d\n",arg1,result);
				} else {
					printf("falha na medida!\n");
				}
			} else {
				printf("uso: medir <arquivo>\n");
			}
			
		} else if(!strcmp(cmd,"criar")) {
			if(args==2) {
				result = fat_create(arg1);
				if(result==0) {
					printf("novo arquivo %s\n",arg1);
				} else {
					printf("falha ao criar arquivo!\n");
				}
			} else {
				printf("uso: criar <arquivo>\n");
			}
		} else if(!strcmp(cmd,"deletar")) {
			if(args==2) {
				if(!fat_delete(arg1)) {
					printf("arquivo %s deletado\n",arg1);
				} else {
					printf("falha na delecao!\n");	
				}
			} else {
				printf("uso: deletar <arquivo>\n");
			}
		} else if(!strcmp(cmd,"ver")) {
			if(args==2) {
				if(!cpout(arg1,"/dev/stdout")) {
					printf("falha em ver arquivo!\n");
				}
			} else {
				printf("uso: ver <nome>\n");
			}

		} else if(!strcmp(cmd,"importar")) {
			if(args==3) {
				if(cpin(arg1,arg2)) {
					printf("arquivo linux %s copiado para %s\n",arg1,arg2);
				} else {
					printf("falha ao copiar!\n");
				}
			} else {
				printf("uso: importar <nome no linux> <nome fat-sys>\n");
			}

		} else if(!strcmp(cmd,"exportar")) {
			if(args==3) {
				if(cpout(arg1,arg2)) {
					printf("fat-sys %s copiado para arquivo %s\n", arg1,arg2);
				} else {
					printf("falha ao copiar!\n");
				}
			} else {
				printf("uso: exportar <nome fat-sys> <nome linux>\n");
			}

		} else if(!strcmp(cmd,"help")) {
			printf("Comandos:\n");
			printf("    formatar\n");
			printf("    montar\n");
			printf("    depurar\n");
			printf("    criar	<arquivo>\n");
			printf("    deletar <arquivo>\n");
			printf("    ver     <arquivo>\n");
			printf("    medir   <arquivo>\n");
			printf("    importar <nome no linux> <nome fat-sys>\n");
			printf("    exportar <nome fat-sys> <nome no linux>\n");
			printf("    help\n");
			printf("    sair\n");
		} else if(!strcmp(cmd,"sair")) {
			break;
		} else {
			printf("comando desconhecido: %s\n",cmd);
			printf("digite 'help'.\n");
			result = 1;
		}
	}

	printf("fechando o disco simulado\n");
	ds_close();

	return 0;
}

int cpin( char *name, char *op_path )
{
	FILE *file;
	int offset=0, result, actual;
	char buffer[16384];

	file = fopen(name,"r");
	if(!file) {
		printf("falha ao acessar %s: %s\n",name,strerror(errno));
		return 0;
	}

	while(1) {
		result = fread(buffer,1,sizeof(buffer),file);
		if(result<=0) break;
		if(result>0) {
			actual = fat_write(op_path,buffer,result,offset);
			if(actual<0) {
				printf("ERRO: fat_write returnou codigo %d\n",actual);
				break;
			}
			offset += actual;
			if(actual!=result) {
				printf("ATENCAO: fat_write escreveu apenas %d bytes, em vez de %d bytes\n",actual,result);
				break;
			}
		}
	}

	printf("copia de %d bytes\n",offset);

	fclose(file);
	return 1;
}

int cpout( char *os_path, char *name )
{
	FILE *file;
	int offset=0, result;
	char buffer[16384];
    if(strcmp(name,"/dev/stdout"))
		file = fopen(name,"w");
	else
		file = stdout;
	if(!file) {
		printf("nao deu para abrir %s: %s\n",name,strerror(errno));
		return 0;
	}

	while(1) {
		result = fat_read(os_path,buffer,sizeof(buffer),offset);
		if(result<=0) break;
		fwrite(buffer,1,result,file);
		offset += result;
	}

	printf("copia de %d bytes\n",offset);

	if(file != stdout)
		fclose(file);
	return 1;
}
