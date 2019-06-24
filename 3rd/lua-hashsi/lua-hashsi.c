#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include "skynet_malloc.h"
#include "hashsi.h"
#include "spinlock.h"

#define SI_MAP_SIZE 20
struct hashsi_map{
	char *name;
	struct hashsi *si;
};

static struct hashsi 		*SI_LIST=NULL;

struct spinlock map_lock={0};
static struct hashsi_map *SI_MAP=NULL;

static int NUM	= 0; 

static int linit(lua_State *L) {
	NUM=lua_rawlen(L,1);
	if(lua_istable(L,1) ==0 || NUM == 0 || SI_LIST != NULL){
		luaL_error(L,"hashsi init error :%d %d %d",lua_istable(L,1),NUM,SI_LIST);
  	}
  	SI_LIST = (struct hashsi *)skynet_malloc(sizeof(struct hashsi)*NUM);
	for(int i=0;i<NUM;i++){
		lua_rawgeti(L,1,i+1);
		int max = luaL_checkinteger(L,-1);
		hashsi_init(&SI_LIST[i],max);
	}
  	return 0;
}

static int lnew(lua_State *L) {
	const char *name=luaL_checkstring(L,1);
	unsigned int  max=luaL_checknumber(L,2);
	spinlock_lock(&map_lock);
	if(SI_MAP == NULL){
		SI_MAP=(struct hashsi_map *)skynet_malloc(sizeof(struct hashsi_map)*SI_MAP_SIZE);
		memset(SI_MAP,0,sizeof(struct hashsi_map)*SI_MAP_SIZE);
	}
	int i=0;
	for(;i<SI_MAP_SIZE;i++){
		if(SI_MAP[i].si==NULL){
			break;
		}
		if(strcmp(SI_MAP[i].name,name)==0){
			spinlock_unlock(&map_lock);
			luaL_error(L,"si_map name exist,name:%s,size:%d",name,SI_MAP_SIZE);
			break;
		}
	}
	if(i==SI_MAP_SIZE){
		spinlock_unlock(&map_lock);
		luaL_error(L,"si_map full,name:%s,size:%d",name,SI_MAP_SIZE);
	}
	SI_MAP[i].si=(struct hashsi *)skynet_malloc(sizeof(struct hashsi));
	SI_MAP[i].name=(char *)skynet_malloc(sizeof(char)*(strlen(name)+1));
	strcpy(SI_MAP[i].name,name);
	hashsi_init(SI_MAP[i].si,max);
	spinlock_unlock(&map_lock);
  	return 0;
}

static struct  hashsi *strid2hashsi(lua_State *L,const char *id){
	struct  hashsi *si=NULL;
	int i=0;
	spinlock_lock(&map_lock);
	if(SI_MAP == NULL){
		spinlock_unlock(&map_lock);
		luaL_error(L,"hashsi id not find:%s",id);
	}
	for(;i<SI_MAP_SIZE;i++){
		if(SI_MAP[i].si==NULL){
			spinlock_unlock(&map_lock);
			luaL_error(L,"hashsi id not find:%s",id);
			break;
		}
		if(strcmp(SI_MAP[i].name,id)==0){
			si=SI_MAP[i].si;
			break;
		}
	}
	spinlock_unlock(&map_lock);
	if(si==NULL){
		luaL_error(L,"hashsi id not find:%s",id);
	}
	return si;
}

static struct  hashsi *id2hashsi(lua_State *L){
	struct  hashsi *si=NULL;
	if(lua_type(L,1)==LUA_TSTRING){
		return strid2hashsi(L,lua_tostring(L,1));
	}
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
		lua_pushinteger(L,node->val);
	}
	rwlock_runlock(&si->lock);
	return 1;
}

static int lset(lua_State *L) {
	struct hashsi * si=id2hashsi(L);
	const char *key=luaL_checkstring(L,2);
	int val;
	int ret;
	rwlock_wlock(&si->lock);
	if(lua_isnil(L,3)){
		hashsi_remove(si,key);
	}else{
		val=luaL_checkinteger(L,3);
		ret=hashsi_upsert(si,key,val);
		if(ret!=0){
			rwlock_wunlock(&si->lock);
			return luaL_error(L,"hashsi insert value error:%d %s %d",ret,key,val);
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
	int index=luaL_checkinteger(L,2);
	for(;index<si->cap;index++){
		if (strlen(si->node[index].key)>0){
			lua_pushinteger(L,index+1);
			lua_pushstring(L,si->node[index].key);
			lua_pushinteger(L,si->node[index].val);
			return 3;
		}
	}
	return 0;
}
static struct luaL_Reg reg[] = {
  {"init", linit},
  {"new", lnew},
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
