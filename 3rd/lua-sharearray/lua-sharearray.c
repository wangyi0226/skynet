#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "atomic.h"
#include "skynet_malloc.h"

#define  SA_TNULL 0
#define  SA_TINT 1
#define  SA_TSTRING 2
#define  SA_TPOINTER 3

struct sharearray{
    void *p;
    int8_t type;
    ATOM_INT ri;    //read index
    ATOM_INT ui;    //update index
    ATOM_INT used;
    int size;
};

static int lnew(lua_State *L){
    int size=luaL_checkinteger(L,1);
    int type=luaL_checkinteger(L,2);
    size_t malloc_size=0;
    struct sharearray *s=(struct sharearray*)skynet_malloc(sizeof(struct sharearray));
    if(s==NULL){
        return luaL_error(L,"sharearray.new error, skynet_malloc failed");
    }
    ATOM_INIT(&s->ri,0);
    ATOM_INIT(&s->ui,0);
    ATOM_INIT(&s->used,0);
    s->ui=0;
    s->used=0;
    s->type=type;
    s->p=NULL;
    s->size=size;
    if(type ==SA_TINT){
       malloc_size=sizeof(int64_t)*size;
    }else if(type == SA_TSTRING){
       malloc_size=sizeof(char *)*size; 
    }else if(type == SA_TPOINTER){
       malloc_size=sizeof(void *)*size; 
    }else{
         return luaL_error(L,"sharearray.new error,unsupported lua type:%d",type);
    }
    s->p=skynet_malloc(malloc_size); 
    memset(s->p,0,malloc_size);
    if(s->p == NULL){
         return luaL_error(L,"sharearray.new error,skynet_malloc failed");
    }
    lua_pushlightuserdata(L,s);
    return 1;
}

static int lupdate(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL || !lua_istable(L,2)){
        return luaL_error(L,"sharearray.update params error"); 
    }
    int i=1;
    int ui=ATOM_LOAD(&s->ui);
    int fail=0;
    while(lua_geti(L,2,i) != LUA_TNIL) {
        if(s->type==SA_TINT){
            lua_Integer v=luaL_checkinteger(L,-1);
            ((int64_t*)s->p)[ui++]=v;
        }else if(s->type==SA_TSTRING){
            char **p=s->p;
            const char *val=luaL_checkstring(L,-1);
            char *new=skynet_malloc(strlen(val)+1);
            if(new==NULL){
                lua_pushfstring(L,"sharearray.update error,index[%d] skynet_malloc error",i);
                fail=1;
                break;
            }
            strcpy(new,val);
            if(p[ui]!=NULL){
                skynet_free(p[ui]);
            }
            p[ui++]=new;
        }else if(s->type==SA_TPOINTER){
            void *p=lua_touserdata(L,-1);
            if(p==NULL){
                lua_pushfstring(L,"sharearray.update error,index[%d] wrong type:%d ",i,lua_type(L,-1));
                fail=1;
                break;
            }
            ((void **)s->p)[ui++]=p;
        }else{
           lua_pushfstring(L,"sharearray type error:%d ",i,s->type);
           fail=1;
           break;
        }
        if(ui>=s->size){
            ui-=s->size;
        }
        i++;
    }
    int update=i-1;
    if(update>0){
        ATOM_STORE(&s->ui,ui);
        //s->used最大值为s->size
        int used=ATOM_LOAD(&s->used);
        if(used<s->size){
            if(used+update>s->size){
                ATOM_STORE(&s->used,s->size);
            }else{
                ATOM_FADD(&s->used,update);
            }
        }
        //s->used小于s->size时,s->ri不变
        if(used+update>=s->size){
            int ri=ATOM_LOAD(&s->ri);
            if(ri+update>=s->size){
                ATOM_STORE(&s->ri,ri+update-s->size);
            }else{
                ATOM_FADD(&s->ri,update);
            }
        }
    }
    if(fail){
        lua_error(L);
    }
    return -1;
}

static int lindex(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL){
        return luaL_error(L,"sharearray.index params error"); 
    }
    int offset=luaL_checkinteger(L,2);
    int ri=ATOM_LOAD(&s->ri);
    int index=(ri+offset)%s->size;
    int used=ATOM_LOAD(&s->used);
    if(index>=used){
        return luaL_error(L,"sharearray.index error,out of range,rindex:%d,uindex:%d,offset:%d,used:%d",ri,ATOM_LOAD(&s->ui),offset,used); 
    }
    lua_pushinteger(L,index);
    return 1;
}

static int lrange(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL){
        return luaL_error(L,"sharearray.range params error"); 
    }
    int i;
    int start=luaL_checkinteger(L,2);
    int end=luaL_checkinteger(L,3);
    int size=end-start;
    int used=ATOM_LOAD(&s->used);
    if(size>s->size|| end>used||size <= 0||start<0){
        return luaL_error(L,"sharearray.range error,out of range,start:%d,end:%d,size:%d,used:%d",start,end,size,used); 
    }
    lua_createtable(L,size,0);
    for(i=0;i<size;i++){
        uint64_t index=(start+i)%s->size;
        if(s->type==SA_TINT){
            lua_pushinteger(L,((int64_t *)s->p)[index]);
        }else {
            void *p=((void **)s->p)[index];
            if(s->type == SA_TSTRING){
                lua_pushstring(L,p);
            }else if(s->type == SA_TPOINTER){
                lua_pushlightuserdata(L,p);
            }else{
                return luaL_error(L,"sharearray.range error,unsupported type:%d",s->type);  
            }
        }
        lua_seti(L,-2,i+1);
    }
    return 1;
}

static int lhas(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL){
        return luaL_error(L,"sharearray.has error, params error"); 
    }
    int i;
    int start=luaL_checkinteger(L,3);
    int end=luaL_checkinteger(L,4);
    int size=end-start;
    int used=ATOM_LOAD(&s->used);
    if(size>s->size|| end>used||size <= 0||start<0){
        return luaL_error(L,"sharearray.has error,out of range,start:%d,end:%d,size:%d,used:%d",start,end,size,used); 
    } 
    int64_t v=0;
    const void *pv=NULL;
    if(s->type==SA_TINT){
        v=luaL_checkinteger(L,2);
    }else if(s->type == SA_TSTRING){
        pv=luaL_checkstring(L,2);
    }else if(s->type == SA_TPOINTER){
        pv=lua_touserdata(L,2);
    }else{
        return luaL_error(L,"sharearray.range error,unsupported type:%d",s->type);  
    }

    for(i=0;i<size;i++){
        int index=(start+i)%s->size;
        if(s->type==SA_TINT){
            if(v == ((int64_t *)s->p)[index]){
                lua_pushboolean(L,1);
                return 1;
            }
        }else if(s->type == SA_TSTRING){
            if(strcmp(((char **)s->p)[index],pv)==0){
                lua_pushboolean(L,1);
                return 1;
            }
        }else if(s->type == SA_TPOINTER){
            if(((void **)s->p)[index]==pv){
                lua_pushboolean(L,1);
                return 1;
            }
        }else{
           return luaL_error(L,"sharearray.range error,unsupported type:%d",s->type);  
        }

    }
    return 0;
}

static int linfo(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL){
        return luaL_error(L,"sharearray.index params error"); 
    }
    lua_createtable(L,0,3);
    lua_pushinteger(L,ATOM_LOAD(&s->ui));
    lua_setfield(L,-2,"ui");
    lua_pushinteger(L,ATOM_LOAD(&s->ri));
    lua_setfield(L,-2,"ri");
    lua_pushinteger(L,ATOM_LOAD(&s->used));
    lua_setfield(L,-2,"used");
    lua_pushinteger(L,s->size);
    lua_setfield(L,-2,"size");
    return 1;
}
static struct luaL_Reg reg[] = {
  {"new",lnew},
  {"update",lupdate},
  {"index",lindex},
  {"range",lrange},
  {"has",lhas},
  {"info",linfo},
  {NULL, NULL}
};


int luaopen_sharearray_core (lua_State *L) {
	luaL_checkversion(L);
	luaL_newlib(L, reg);
  	return 1;
}
