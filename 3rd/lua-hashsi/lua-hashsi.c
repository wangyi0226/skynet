#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include "skynet_malloc.h"
#include "hashsi.h"
#include "spinlock.h"

#define SI_MAP_SIZE 20
#define PUSH_VAL(L,node)if((node)->sv!=NULL){lua_pushstring(L,(node)->sv);}else{lua_pushinteger(L,(node)->iv);}
		

static struct hashsi 		*SI_LIST=NULL;
static int NUM	= 0; 

static int linit(lua_State *L) {
	NUM=lua_rawlen(L,1);
	if(lua_istable(L,1) ==0 || NUM == 0 || SI_LIST != NULL){
		luaL_error(L,"hashsi init error :%d %d %d",lua_istable(L,1),NUM,SI_LIST);
  	}
  	SI_LIST = (struct hashsi *)skynet_malloc(sizeof(struct hashsi)*NUM);
	int i=0;
	for(i=0;i<NUM;i++){
		lua_rawgeti(L,1,i+1);
		int max = luaL_checkinteger(L,-1);
		hashsi_init(&SI_LIST[i],max);
	}
  	return 0;
}
static struct  hashsi *id2hashsi(lua_State *L){
	struct  hashsi *si=NULL;
	int id=luaL_checkinteger(L,1);
	id=id-1;
	if(id >= NUM || id<0){
		luaL_error(L,"hashsi id error:%d",id);
	}else{
		si=&SI_LIST[id];
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
	lua_Integer ival=-1;
	const char *sval=NULL;
	int ret;
	rwlock_wlock(&si->lock);
	if(lua_isnil(L,3)){
		hashsi_remove(si,key);
	}else{
		int tp=lua_type(L,3);
		if(tp == LUA_TSTRING){
			sval=luaL_checkstring(L,3);
		}else{
			ival=luaL_checkinteger(L,3);
		}
		ret=hashsi_upsert(si,key,ival,sval);
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
  {NULL, NULL}
};


int luaopen_hashsi_core (lua_State *L) {
	luaL_checkversion(L);
	luaL_newlib(L, reg);
  	return 1;
}
