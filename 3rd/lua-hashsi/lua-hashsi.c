#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include "skynet_malloc.h"
#include "hashsi.h"
#include "spinlock.h"

#define SI_MAP_SIZE 20
#define PUSH_VAL(L,node)if(node->type==HASHSI_TSTRING){lua_pushstring(L,node->val.p);}else if(node->type == HASHSI_TINT){lua_pushinteger(L,(node)->val.n);}else if(node->type == HASHSI_TPOINTER){lua_pushlightuserdata(L,(node)->val.p);}

struct hashsi_kv{
    char *key;
    struct hashsi *si;
};

struct hashsi_map{
    struct hashsi_kv *m;
    int used;
    int len;
};

static struct hashsi 		*SI_LIST=NULL;
static struct hashsi_map    *SI_MAP=NULL;
static struct rwlock lock={0,0};

static int NUM	= 0; 

static int linit(lua_State *L) {
    rwlock_wlock(&lock);
	NUM=lua_rawlen(L,1);
	if(lua_istable(L,1) ==0 || NUM == 0 || SI_LIST != NULL){
        rwlock_wunlock(&lock);
		luaL_error(L,"hashsi init error :%d %d %d",lua_istable(L,1),NUM,SI_LIST);
  	}
  	SI_LIST = (struct hashsi *)skynet_malloc(sizeof(struct hashsi)*NUM);
	int i=0;
	int max_cap = luaL_checkinteger(L,2);
	for(i=0;i<NUM;i++){
		lua_rawgeti(L,1,i+1);
		int node_size = luaL_checkinteger(L,-1);
		hashsi_init(&SI_LIST[i],node_size,max_cap);
	}
    rwlock_wunlock(&lock);
  	return 0;
}

static int lnew(lua_State *L){
    const char *key= luaL_checkstring(L,1);
    int i=0;
    rwlock_wlock(&lock);
    if(SI_MAP==NULL){
        SI_MAP = (struct hashsi_map *)skynet_malloc(sizeof(struct hashsi_map));
        SI_MAP->len=10;
        SI_MAP->used=0;
        SI_MAP->m=(struct hashsi_kv*)skynet_malloc(sizeof(struct hashsi_kv)*SI_MAP->len);
        memset(SI_MAP->m, 0, SI_MAP->len * sizeof(struct hashsi_kv));
    }
    for(i=0;i<SI_MAP->used;i++){
        if(strcmp(SI_MAP->m[i].key,key)==0){
            luaL_error(L,"hashsi new error,key[%s] already exists",key);
            return 0;
        }
    }
    if(SI_MAP->len == SI_MAP->used){
        int new_len=SI_MAP->len*2;
        struct  hashsi_kv * m=(struct hashsi_kv*)skynet_malloc(sizeof(struct hashsi_kv)*new_len);
        memset(m, 0,  sizeof(struct hashsi_kv )*new_len);
        memcpy(m,SI_MAP->m,sizeof (struct hashsi_kv)*SI_MAP->len);
        skynet_free(SI_MAP->m);
        SI_MAP->m=m;
        SI_MAP->len=new_len;
    }
    struct hashsi_kv * kv=&SI_MAP->m[SI_MAP->used];
    kv->key= skynet_malloc(strlen(key)+1);
    strcpy(kv->key,key);
    kv->si= hashsi_new(0,0);
    SI_MAP->used++;
    rwlock_wunlock(&lock);
    return 0;
}

static struct  hashsi *id2hashsi(lua_State *L){
	struct  hashsi *si=NULL;
    if(lua_isinteger(L,1)){
        int id=luaL_checkinteger(L,1);
        id=id-1;
        if(id >= NUM || id<0){
            luaL_error(L,"hashsi id error:%d",id);
        }else{
            si=&SI_LIST[id];
        }
    }else{
        rwlock_rlock(&lock);
        const char *key= luaL_checkstring(L,1);
        int i=0;
        for(i=0;i<SI_MAP->used;i++){
            if(strcmp(SI_MAP->m[i].key,key)==0){
                rwlock_runlock(&lock);
                return SI_MAP->m[i].si;
            }
        }
        rwlock_runlock(&lock);
        luaL_error(L,"hashsi key[%s] does not exist",key);
        return  NULL;
    }

	return si;
}

static int lget(lua_State *L) {
	struct hashsi * si=id2hashsi(L);
	const char *key=luaL_checkstring(L,2);
	rwlock_rlock(&si->lock);
	struct hashsi_node *node=hashsi_lookup(si,key);
	if(node==NULL){
		lua_pushnil(L);
	}
	else{
		PUSH_VAL(L,node)
	}
	rwlock_runlock(&si->lock);
	return 1;
}

static int lset(lua_State *L) {
	struct hashsi * si=id2hashsi(L);
	const char *key=luaL_checkstring(L,2);
	int ret;
	rwlock_wlock(&si->lock);
	if(lua_isnil(L,3)){
		hashsi_remove(si,key);
	}else{
		int tp=lua_type(L,3);
		if(tp == LUA_TNUMBER){
            int64_t n=luaL_checkinteger(L,3);
            ret=hashsi_upsert(si,key,HASHSI_TINT,&n);
		}else if(tp == LUA_TSTRING){
            const char *s=luaL_checkstring(L,3);
            char *p= skynet_malloc(strlen(s)+1);
            strcpy(p,s);
            ret=hashsi_upsert(si,key,HASHSI_TSTRING,p);
		}else if(tp == LUA_TLIGHTUSERDATA){
            void *p= lua_touserdata(L,3);
            if(p==NULL){
                return luaL_error(L,"hashsi insert error:%s,userdata value is null",key);
            }
            ret=hashsi_upsert(si,key,HASHSI_TPOINTER,p);
        }
        else{
            return luaL_error(L,"hashsi insert value error,key:%s,unsupported lua type:%d",key,tp);
        }
		if(ret!=0){
			rwlock_wunlock(&si->lock);
			return luaL_error(L,"hashsi insert value error:%d %s",ret,key);
		}
	}
	rwlock_wunlock(&si->lock);
	return 0;
}

static int lcount(lua_State *L) {
	struct hashsi * si=id2hashsi(L);
	lua_pushinteger(L,si->count);
	return 1;
}

static int lnext(lua_State *L) {
	struct hashsi * si=id2hashsi(L);
	int i = luaL_checkinteger(L,2);
	int j = luaL_checkinteger(L,3);
	rwlock_rlock(&si->lock);
	int index,count=0;
	for(index=i;index<si->cap;index++){
		struct hashsi_node * node=si->hash[index];
		count=0;
		while (node !=NULL){
			if(count>j){
				lua_pushinteger(L,index);
				lua_pushinteger(L,count);
				lua_pushstring(L,node->key);
				PUSH_VAL(L,node)
				rwlock_runlock(&si->lock);
				return 4;
			}
			node=node->next;
			count++;
		}
		j=-1;
	}
	rwlock_runlock(&si->lock);
	return 0;
}
static struct luaL_Reg reg[] = {
  {"init", linit},
  {"set",lset},
  {"get",lget},
  {"count",lcount},
  {"next",lnext},
  {"new",lnew},
  {NULL, NULL}
};


int luaopen_hashsi_core (lua_State *L) {
	luaL_checkversion(L);
	luaL_newlib(L, reg);
  	return 1;
}
