#include <lua.h>
#include <lauxlib.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "../mini.h"

#define MN_MT "mini"
#define MN_ITER_MT "mini.iter"

static int mn_lua_encode(lua_State *lua)
{
   const char *path = luaL_checkstring(lua, 1);
   luaL_argcheck(lua, lua_isfunction(lua, 2), 2, "expected a callable");

   static const char *const types[] = {
      [MN_STANDARD] = "standard",
      [MN_NUMBERED] = "numbered",
      NULL
   };
   enum mn_type type = luaL_checkoption(lua, 3, "standard", types);
   
   struct mini_enc *enc = mn_enc_new(type);

   FILE *fp = NULL;
   
   for (;;) {
      /* Copy the iterator. */
      lua_pushvalue(lua, 2);
      if (lua_pcall(lua, 0, 1, 0))
         /* Error message at the top of the stack. */
         goto fail;
      if (lua_isnil(lua, -1))
         break;
      if (lua_type(lua, -1) != LUA_TSTRING) {
         lua_pushliteral(lua, "callback function didn't return a string");
         goto fail;
      } 
      size_t len;
      const char *word = lua_tolstring(lua, -1, &len);
      int ret = mn_enc_add(enc, word, len);
      if (ret) {
         lua_pushfstring(lua, "error while adding '%s': %s", word, mn_strerror(ret));
         goto fail;
      }
      lua_pop(lua, 1);
   }
   
   fp = fopen(path, "wb");
   if (!fp) {
      lua_pushstring(lua, strerror(errno));
      goto fail;
   }
   int ret = mn_enc_dump_file(enc, fp);
   if (ret) {
      lua_pushstring(lua, mn_strerror(ret));
      goto fail;
   }
   if (fclose(fp)) {
      lua_pushstring(lua, strerror(errno));
      fp = NULL;
      goto fail;
   }
   mn_enc_free(enc);
   return 0;

fail:
   mn_enc_free(enc);
   if (fp)
      fclose(fp);
   return lua_error(lua);
}

struct mini_lua {
   struct mini *fsa;
   int lua_ref;
   int ref_cnt;
};

static int mn_lua_load(lua_State *lua)
{
   const char *path = luaL_checkstring(lua, 1);
   struct mini_lua *fsa = lua_newuserdata(lua, sizeof *fsa);

   FILE *fp = fopen(path, "rb");
   if (!fp) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }
   
   int ret = mn_load_file(&fsa->fsa, fp);
   fclose(fp);
   if (ret) {
      lua_pushnil(lua);
      lua_pushstring(lua, mn_strerror(ret));
      return 2;
   }

   fsa->lua_ref = LUA_NOREF;
   fsa->ref_cnt = 0;
   luaL_getmetatable(lua, MN_MT);
   lua_setmetatable(lua, -2);
   return 1;
}

static int mn_lua_free(lua_State *lua)
{
   struct mini_lua *fsa = luaL_checkudata(lua, 1, MN_MT);
   mn_free(fsa->fsa);
   assert(fsa->ref_cnt == 0 && fsa->lua_ref == LUA_NOREF);
   return 0;
}

static const struct mini *check_fsa(lua_State *lua)
{
   const struct mini_lua *fsa = luaL_checkudata(lua, 1, MN_MT);
   return fsa->fsa;
}

static int mn_lua_contains(lua_State *lua)
{
   const struct mini *fsa = check_fsa(lua);
   size_t len;
   const char *word = luaL_checklstring(lua, 2, &len);
   lua_pushboolean(lua, mn_contains(fsa, word, len));
   return 1;
}

static int mn_lua_extract(lua_State *lua)
{
   const struct mini *fsa = check_fsa(lua);
   uint32_t pos = luaL_checknumber(lua, 2);
   
   char word[MN_MAX_WORD_LEN + 1];
   size_t len = mn_extract(fsa, pos, word);
   if (len)
      lua_pushlstring(lua, word, len);
   else
      lua_pushnil(lua);
   return 1;
}

static int mn_lua_locate(lua_State *lua)
{
   const struct mini *fsa = check_fsa(lua);
   size_t len;
   const char *word = luaL_checklstring(lua, 2, &len);
   
   uint32_t pos = mn_locate(fsa, word, len);
   if (pos)
      lua_pushnumber(lua, pos);
   else
      lua_pushnil(lua);
   return 1;
}

static int mn_lua_type(lua_State *lua)
{
   const struct mini *fsa = check_fsa(lua);
   
   enum mn_type t = mn_type(fsa);
   switch (t) {
   case MN_STANDARD:
   default:
      assert(t == MN_STANDARD);
      lua_pushliteral(lua, "standard");
      break;
   case MN_NUMBERED:
      lua_pushliteral(lua, "numbered");
      break;
   }
   return 1;
}

static int mn_lua_size(lua_State *lua)
{
   const struct mini *fsa = check_fsa(lua);
   lua_pushnumber(lua, mn_size(fsa));
   return 1;
}

struct mini_lua_iter {
   struct mini_iter it;
   struct mini_lua *fsa;
};

static int mn_lua_iter_next(lua_State *lua);

static struct mini_iter *mn_lua_iter_new(lua_State *lua, struct mini **fsap)
{
   struct mini_lua *fsa = luaL_checkudata(lua, 1, MN_MT);
   struct mini_lua_iter *it = lua_newuserdata(lua, sizeof *it);

   if (fsa->ref_cnt++ == 0) {
      lua_pushvalue(lua, 1);
      fsa->lua_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
   }
   it->fsa = fsa;
   
   luaL_getmetatable(lua, MN_ITER_MT);
   lua_setmetatable(lua, -2);
   
   *fsap = fsa->fsa;
   return &it->it;
}

static int mn_lua_iter_init(lua_State *lua)
{
   struct mini *fsa;
   struct mini_iter *it = mn_lua_iter_new(lua, &fsa);
   
   switch (lua_type(lua, 2)) {
   case LUA_TNUMBER:
      mn_iter_initn(it, fsa, lua_tonumber(lua, 2));
      break;
   case LUA_TSTRING: {
      size_t len;
      const char *str = lua_tolstring(lua, 2, &len);
      mn_iter_inits(it, fsa, str, len);
      break;
   }
   default:
      mn_iter_init(it, fsa);
      break;
   }
   
   lua_pushcclosure(lua, mn_lua_iter_next, 1);
   return 1;
}

static int mn_lua_iter_prefixed(lua_State *lua)
{
   size_t len;
   const char *prefix = luaL_checklstring(lua, 2, &len);

   struct mini *fsa;
   struct mini_iter *it = mn_lua_iter_new(lua, &fsa);
   mn_iter_initp(it, fsa, prefix, len);
   
   lua_pushcclosure(lua, mn_lua_iter_next, 1);
   return 1;
}

static int mn_lua_iter_next(lua_State *lua)
{
   struct mini_iter *it = lua_touserdata(lua, lua_upvalueindex(1));
   size_t len;
   const char *word = mn_iter_next(it, &len);
   if (word) {
      lua_pushlstring(lua, word, len);
      return 1;
   }
   return 0;
}

static int mn_lua_iter_fini(lua_State *lua)
{
   struct mini_lua_iter *it = luaL_checkudata(lua, 1, MN_ITER_MT);
   struct mini_lua *fsa = it->fsa;
   if (--fsa->ref_cnt == 0) {
      luaL_unref(lua, LUA_REGISTRYINDEX, fsa->lua_ref);
      fsa->lua_ref = LUA_NOREF;
   }
   return 0;
}

int luaopen_mini(lua_State *lua)
{
   const luaL_Reg fns[] = {
      {"__gc", mn_lua_free},
      {"__len", mn_lua_size},
      {"locate", mn_lua_locate},
      {"extract", mn_lua_extract},
      {"contains", mn_lua_contains},
      {"type", mn_lua_type},
      {"size", mn_lua_size},
      {"iter", mn_lua_iter_init},
      {"iter_prefixed", mn_lua_iter_prefixed},
      {NULL, NULL},
   };
   luaL_newmetatable(lua, MN_MT);
   lua_pushvalue(lua, -1);
   lua_setfield(lua, -2, "__index");
   luaL_setfuncs(lua, fns, 0);
   
   luaL_newmetatable(lua, MN_ITER_MT);
   lua_pushliteral(lua, "__gc");
   lua_pushcfunction(lua, mn_lua_iter_fini);
   lua_settable(lua, -3);
   
   const luaL_Reg lib[] = {
      {"load", mn_lua_load},
      {"encode", mn_lua_encode},
      {NULL, NULL},
   };
   luaL_newlib(lua, lib);
   lua_pushnumber(lua, MN_MAX_WORD_LEN);
   lua_setfield(lua, -2, "MAX_WORD_LEN");
   
   return 1;
}
