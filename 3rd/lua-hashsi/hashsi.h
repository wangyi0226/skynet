#ifndef skynet_hashsi_h
#define skynet_hashsi_h

#include "rwlock.h"

#define HASHSI_KEYLEN	50
struct hashsi_node {
	char key[HASHSI_KEYLEN+1];
	int64_t val;
	struct hashsi_node *next;
};

struct hashsi {
	struct rwlock lock;
	int hashmod;
	int cap;
	int count;
	struct hashsi_node *node;
	struct hashsi_node **hash;
};

void hashsi_init(struct hashsi *si, int max);
void hashsi_clear(struct hashsi *si);
struct hashsi_node *hashsi_lookup(struct hashsi *si, const char * key);
void hashsi_remove(struct hashsi *si, const char * key);
int hashsi_upsert(struct hashsi * si,const char * key,int64_t val); 
int hashsi_full(struct hashsi *si);
#endif
