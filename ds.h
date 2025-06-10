#define BLOCK_SIZE 4096

int  ds_init( const char *filename, int number_blocks );
int  ds_size();
void ds_read( int number, char *buff );
void ds_write( int number, const char *buff );
void ds_close();
