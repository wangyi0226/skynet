#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "skynet_malloc.h"
#include "hashsi.h"
#define INIT_NODE(node)(node).val.p=NULL;(node).key=NULL;(node).next=NULL;(node).type=HASHSI_TNULL;
#define FREE_VALUE(v) if(v->type == HASHSI_TSTRING){skynet_free(v->val.p);}v->type=HASHSI_TNULL;v->val.p=NULL;
#define START_HASHCAP 16
#define MAX_CAP 65536

unsigned int lhash(const char *key){
	unsigned int l = strlen(key);
	unsigned int h = 1559042055 ^ l;
	size_t step = (l >>5) + 1;
	for (; l >= step; l -= step){
		h ^= ((h<<5) + (h>>2) + (unsigned char)(key[l - 1]));
	}
	return h;
}

void rehash(struct hashsi *si){
	if(si->count<=START_HASHCAP || si->cap>=si->max_cap){
		return;
	}
	int newcap=0;
	if(si->count >= si->cap){
		newcap = si->cap*2;
	}else if (si->count*3<=si->cap){
		newcap = si->cap/2;
	}
	if(newcap==0){
		return;
	}
	int newmod=newcap-1;
	struct hashsi_node ** newhash= skynet_malloc(newcap * sizeof(struct hashsi_node *));
	memset(newhash, 0, newcap * sizeof(struct hashsi_node *));
	for(int i=0;i<si->cap;i++){
		struct hashsi_node *next=si->hash[i];
		while(next!=NULL){
			struct hashsi_node *old=next;
			next=old->next;
			unsigned int h=lhash(old->key)&newmod;
			old->next = newhash[h];
			newhash[h]=old;
		}
	}
	skynet_free(si->hash);
	si->hash=newhash;
	si->cap=newcap;
	si->hashmod=newmod;
}

struct hashsi *hashsi_new(int node_size,int max_cap){
	struct hashsi *si =	(struct hashsi *)skynet_malloc(sizeof(struct hashsi));
	hashsi_init(si,node_size,max_cap);
	return si;
}

void hashsi_init(struct hashsi *si, int node_size,int max_cap) {
	int i;
	int hashcap;
	hashcap = START_HASHCAP;
	while (hashcap < node_size) {
		hashcap *= 2;
	}
	rwlock_init(&si->lock);
	si->hashmod = hashcap - 1;
	si->cap = hashcap;
	si->node_size = node_size;
	si->count = 0;
	if (max_cap!=0){
		si->max_cap = max_cap;
	}else{
		si->max_cap = MAX_CAP;
	}

	if (node_size == 0) {
	    si->node=NULL;
	}else{
	    si->node = skynet_malloc(node_size * sizeof(struct hashsi_node));
	    for (i=0;i<node_size;i++) {
	        INIT_NODE(si->node[i])
	    }
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
	if (strcmp(c->key,key) ==0) {
		si->hash[h] = c->next;
		goto _clear;
	}
	struct hashsi_node * next = c->next;
	while(next) {
		if (strcmp(next->key,key)==0){
			c->next = next->next;
			c = next;
			goto _clear;
		}
		c=next;
		next = next->next;
	}

	return;
_clear:
	skynet_free(c->key);
	c->key=NULL;
    FREE_VALUE(c)
	c->next = NULL;
	if (si->node == NULL){
		skynet_free(c);	
	}
	--si->count;
	rehash(si);
	return ;
}


int hashsi_upsert(struct hashsi * si, const char * key,int type,void *p) {
	struct hashsi_node *c = hashsi_lookup(si,key);
	if(c!=NULL){
        FREE_VALUE(c)
        goto _update;
	}
	int keylen=strlen(key);
	if(keylen>MAX_HASHSI_KEYLEN || keylen == 0){
		return 2;
	}
	unsigned int h=lhash(key);
	int i;
	if(si->node == NULL){
		rehash(si);
		c = skynet_malloc(sizeof(struct hashsi_node));
		if(c==NULL){
			return 3;
		}
		INIT_NODE(*c)
	}else{
		if(hashsi_full(si)){
			return 1;
		}
		for (i=0;i<si->cap;i++) {
			unsigned int index = (i+h) % si->node_size;
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
	}
	++si->count;
	c->key = skynet_malloc(keylen+1);
	strcpy(c->key,key);
	h = h & si->hashmod;
	c->next = si->hash[h];
	si->hash[h] = c;

_update:
    c->type=type;
    if(type == HASHSI_TINT){
        c->val.n=*(int64_t*)p;
    }else if(type == HASHSI_TSTRING || type == HASHSI_TPOINTER){
        c->val.p=p;
    }else {
        return 5;
    }
	return 0;
}

int hashsi_full(struct hashsi *si) {
	return si->count == si->node_size;
}

