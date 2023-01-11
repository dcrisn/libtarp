#ifndef DEMONIC_H
#define DEMONIC_H

#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * This is a lua library making available standard posix system calls 
 * to make daemonization of LUA scripts straightforward;
 * 
 * The basic sequence of calls to perform is the usual :
 * fork(), setsid(), fork() again (recommented);
 */

// call to setsid
int mosetsid(lua_State *L);
// call to fork
int mofork(lua_State *L);

// lib opener
int luaopen_demonic(lua_State *L);
#endif
