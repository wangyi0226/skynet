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

#define check_error(s) if(ATOM_LOAD(&s->error)!=0){return luaL_error(L,"sharearray call failed,an error occurred");}
#define check_init(s) if(s->p ==NULL){return luaL_error(L,"sharearray uninitialized");}
struct sharearray{
    void *p;
    int8_t type;
    ATOM_INT ri;    //read index
    ATOM_INT ui;    //update index
    ATOM_INT error;
    int size;
};

static int lnew(lua_State *L){
    int size=luaL_checkinteger(L,1);
    int type=luaL_checkinteger(L,2);
    struct sharearray *s=(struct sharearray*)skynet_malloc(sizeof(struct sharearray));
    if(s==NULL){
        return luaL_error(L,"sharearray.new error, skynet_malloc failed");
    }
    ATOM_INIT(&s->ri,0);
    ATOM_INIT(&s->ui,0);
    ATOM_INIT(&s->error,0);
    s->type=type;
    s->p=NULL;
    s->size=size;
    lua_pushlightuserdata(L,s);
    return 1;
}

static int linit(lua_State *L){
    struct sharearray *s=(struct sharearray*)lua_touserdata(L,1);
    if(s==NULL || !lua_istable(L,2)){
        return luaL_error(L,"sharearray.init params error"); 
    }
    int sri=lua_tointeger(L,3);
    int sui=lua_tointeger(L,4);
    if(sri>=s->size||sui>=s->size||sri<0||sui<0||sri==sui){
        ATOM_STORE(&s->error,1);
        return luaL_error(L,"sharearray.init params error: ui %d,ri:%d, sharearray size:%d",sui,sri,s->size); 
    }
    if(s->p!=NULL){
        return luaL_error(L,"sharearray.init error,sharearray already initialized"); 
    }
    check_error(s)
    int len=luaL_len(L,2);
    if(len!=s->size){
        ATOM_STORE(&s->error,1);
        return luaL_error(L,"sharearray.init list_len must be %d,list_len:%d",s->size,len);
    }
    size_t malloc_size=0;
    if(s->type ==SA_TINT){
       malloc_size=sizeof(int32_t)*s->size;
    }else if(s->type == SA_TSTRING){
       malloc_size=sizeof(char *)*s->size; 
    }else if(s->type == SA_TPOINTER){
       malloc_size=sizeof(void *)*s->size; 
    }else{
        ATOM_STORE(&s->error,1);
        return luaL_error(L,"sharearray.new error,unsupported lua type:%d",s->type);
    }
    void *sp=skynet_malloc(malloc_size); 
    memset(sp,0,malloc_size);
    if(sp == NULL){
        ATOM_STORE(&s->error,1);
        return luaL_error(L,"sharearray.new error,skynet_malloc failed");
    }

    int i=1;
    int ui=0;
    int fail=0;
    while(lua_geti(L,2,i) != LUA_TNIL) {
        if(s->type==SA_TINT){
            int32_t *p=sp;
            lua_Integer v=luaL_checkinteger(L,-1);
            p[ui++]=v;
        }else if(s->type==SA_TSTRING){
            char **p=sp;
            const char *val=luaL_checkstring(L,-1);
            char *new=skynet_malloc(strlen(val)+1);
            if(new==NULL){
                lua_pushfstring(L,"sharearray.update error,index[%d] skynet_malloc error",i);
                fail=1;
                break;
            }
            strcpy(new,val);
            p[ui++]=new;
        }else if(s->type==SA_TPOINTER){
            void *p=lua_touserdata(L,-1);
            if(p==NULL){
                lua_pushfstring(L,"sharearray.update error,index[%d] wrong type:%d ",i,lua_type(L,-1));
                fail=1;
                break;
            }
            ((void **)sp)[ui++]=p;
        }else{
           lua_pushfstring(L,"sharearray type error:%d ",i,s->type);
           fail=1;
           break;
        }
        i++;
        lua_pop(L,1);
    }

    if(fail){
        lua_pop(L,1);
        ATOM_STORE(&s->error,1);
        if(s->type==SA_TSTRING){
            char **p=sp;
            for(i=0;i<ui;i++){
                if(p[i]!=NULL){
                    skynet_free(p[i]);
                }
            }
        }
        skynet_free(sp);
        return lua_error(L);
    }else{
        s->p=sp;
        ATOM_STORE(&s->ri,sri);
        ATOM_STORE(&s->ui,sui);
    }
    return 0;
}

static int lupdate(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL || !lua_istable(L,2)){
        return luaL_error(L,"sharearray.update params error"); 
    }
    check_init(s)
    check_error(s)
    int i=1;
    int ui=ATOM_LOAD(&s->ui);
    int fail=0;
    while(lua_geti(L,2,i) != LUA_TNIL) {
        if(s->type==SA_TINT){
            lua_Integer v=luaL_checkinteger(L,-1);
            ((int32_t*)s->p)[ui++]=v;
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
        int ri=ATOM_LOAD(&s->ri);
        if(ri+update>=s->size){
            ATOM_STORE(&s->ri,ri+update-s->size);
        }else{
            ATOM_FADD(&s->ri,update);
        }
    }
    if(fail){
        lua_error(L);
        return -1;
    }
    return 0;
}

static int lindex(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL){
        return luaL_error(L,"sharearray.index params error"); 
    }
    check_init(s)
    check_error(s)
    int offset=luaL_checkinteger(L,2);
    int ri=ATOM_LOAD(&s->ri);
    int index=(ri+offset)%s->size;
    lua_pushinteger(L,index);
    return 1;
}

static int lrange(lua_State *L){
    struct sharearray *s=(struct sharearray *)lua_touserdata(L,1);
    if(s==NULL){
        return luaL_error(L,"sharearray.range params error"); 
    }
    check_init(s)
    int i;
    int start=luaL_checkinteger(L,2);
    int end=luaL_checkinteger(L,3);
    int size=end-start;
    if(size>s->size|| size <= 0||start<0){
        return luaL_error(L,"sharearray.range error,out of range,start:%d,end:%d,size:%d",start,end,size); 
    }
    lua_createtable(L,size,0);
    for(i=0;i<size;i++){
        int index=(start+i)%s->size;
        if(s->type==SA_TINT){
            lua_pushinteger(L,((int32_t *)s->p)[index]);
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
    check_init(s)
    int i;
    int start=luaL_checkinteger(L,3);
    int end=luaL_checkinteger(L,4);
    int size=end-start;
    if(size>s->size ||size <= 0 || start<0){
        return luaL_error(L,"sharearray.has error,out of range,start:%d,end:%d,size:%d",start,end,size); 
    } 
    int32_t v=0;
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
            if(v == ((int32_t *)s->p)[index]){
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
    lua_pushinteger(L,s->size);
    lua_setfield(L,-2,"size");
    lua_pushinteger(L,s->error);
    lua_setfield(L,-2,"error");
    return 1;
}
static struct luaL_Reg reg[] = {
  {"new",lnew},
  {"init",linit},
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
