#include "demonic.h"

int mofork(lua_State *L){
     switch(fork()){  // become background process
        case -1:  // error
            lua_pushinteger(L,-1);
            return 1;
        case 0:  // child should continue (i.e. return to lua)
            lua_pushinteger(L,0);
            return 1;
            break;
        default: exit(EXIT_SUCCESS); // parent should exit
    }
}


int mosetsid(lua_State *L){
    int res = 0;
    if (setsid() == -1){ 
        res = -1;
    }
    lua_pushinteger(L,res);
    return 1;
}


const struct luaL_Reg demonic[]= 
{
    {"fork", mofork},
    {"setsid", mosetsid},
    {NULL, NULL}
};

int luaopen_demonic(lua_State *L){
    luaL_newlib(L, demonic);
    return 1;
}
