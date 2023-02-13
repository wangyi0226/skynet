#ifndef skynet_hashsi_h
#define skynet_hashsi_h
#include <stdint.h>
#include "rwlock.h"

#define MAX_HASHSI_KEYLEN	100
#define  HASHSI_TNULL 0
#define  HASHSI_TINT 1
#define  HASHSI_TSTRING 2
#define  HASHSI_TPOINTER 3

union hashsi_val{
    union {
        int64_t n;
        void *p;
    };
};

struct hashsi_node {
	char *key;
    struct hashsi_node *next;
    int type;
    union hashsi_val val;
};

struct hashsi {
	struct rwlock lock;
	int hashmod;
	int cap;
	int count;

	int node_size;
	struct hashsi_node *node;

	int max_cap;
	struct hashsi_node **hash;
};

void hashsi_init(struct hashsi *si, int node_size, int max_cap);
struct hashsi * hashsi_new( int node_size, int max_cap);

//void hashsi_clear(struct hashsi *si);
struct hashsi_node *hashsi_lookup(struct hashsi *si, const char * key);
void hashsi_remove(struct hashsi *si, const char * key);
int hashsi_upsert(struct hashsi * si,const char * key,int type,void *p);
int hashsi_full(struct hashsi *si);
#endif
