#include "streebog.h"
#include <crypt/gost_r_34_11-2012.h>

typedef struct gost_ctx
{
	void *ctx;
	void (*test)();
	void (*init)();
	void (*update)();
	void (*final)();
	unsigned char *hash;
	unsigned version;
	unsigned hash_size;
} gost_ctx;