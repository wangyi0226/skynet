#include <stdlib.h>
#include <string.h>
#include "skynet_malloc.h"
#include "hashsi.h"

unsigned int lhash(const char *key){
	unsigned int l = strlen(key);
	unsigned int h = 1559042055 ^ l;
	size_t step = (l >>5) + 1;
	for (; l >= step; l -= step){
		h ^= ((h<<5) + (h>>2) + (unsigned char)(key[l - 1]));
	}
	return h;
}

void hashsi_init(struct hashsi *si, int max) {
	int i;
	int hashcap;
	hashcap = 16;
	while (hashcap < max) {
		hashcap *= 2;
	}
	rwlock_init(&si->lock);
	si->hashmod = hashcap - 1;
	si->cap = max;
	si->count = 0;
	si->node = skynet_malloc(max * sizeof(struct hashsi_node));
	for (i=0;i<max;i++) {
		si->node[i].val = -1;
		si->node[i].key=NULL;
		si->node[i].next = NULL;
	}
	si->hash = skynet_malloc(hashcap * sizeof(struct hashsi_node *));
	memset(si->hash, 0, hashcap * sizeof(struct hashsi_node *));
}

/*
void hashsi_clear(struct hashsi *si) {
	//skynet_free(si->node);需要释放node中所有key
	skynet_free(si->hash);
	si->node = NULL;
	si->hash = NULL;
	si->hashmod = 1;
	si->cap = 0;
	si->count = 0;
}
*/

struct hashsi_node * hashsi_lookup(struct hashsi *si, const char *key) {
	unsigned int h =lhash(key)&si->hashmod;
	struct hashsi_node * c = si->hash[h];
	while(c) {
		if(c->key != NULL){
			int r = strcmp(c->key, key);
			if (r==0){
				return c;
			}
		}
		c = c->next;
	}
	return NULL;
}

void hashsi_remove(struct hashsi *si,const char *key) {
	unsigned int h =lhash(key)&si->hashmod;
	struct hashsi_node * c = si->hash[h];
	if (c == NULL){
		return ;
	}
	if (c->key!=NULL && strcmp(c->key,key) ==0) {
		si->hash[h] = c->next;
		goto _clear;
	}
	while(c->next) {
		if (c->next->key!=NULL && strcmp(c->next->key,key)==0){
			struct hashsi_node * temp = c->next;
			c->next = temp->next;
			c = temp;
			goto _clear;
		}
		c = c->next;
	}
	return;
_clear:
	skynet_free(c->key);
	c->key=NULL;
	c->val=0;
	c->next = NULL;
	--si->count;
	return ;
}

int hashsi_upsert(struct hashsi * si, const char * key,int64_t val) {

	struct hashsi_node *c = hashsi_lookup(si,key);
	if(c!=NULL){
		c->val = val;
		return 0;
	}
	if(hashsi_full(si)){
		return 1;
	}
	int keylen=strlen(key);
	if(keylen>MAX_HASHSI_KEYLEN || keylen == 0){
		return 2;
	}
	unsigned int h=lhash(key);
	int i;
	for (i=0;i<si->cap;i++) {
		unsigned int index = (i+h) % si->cap;
		if (si->node[index].key==NULL) {
			c = &si->node[index];
			break;
		}
	}
	if(c==NULL){
		return 3;
	}
	if(c->next != NULL){
		return 4;
	}
	++si->count;
	c->key = skynet_malloc(keylen+1);
	strcpy(c->key,key);
	c->val = val;
	h = h & si->hashmod;
	if (si->hash[h]) {
		c->next = si->hash[h];
	}
	si->hash[h] = c;
	return 0;
}

int hashsi_full(struct hashsi *si) {
	return si->count == si->cap;
}

