
void fat_debug();
int  fat_format();
int  fat_mount();

int  fat_create( char *name);
int  fat_delete( char *name );
int  fat_getsize( char *name);

int  fat_read( char *name, char *buff, int length, int offset );
int  fat_write( char *name, const char *buff, int length, int offset );
