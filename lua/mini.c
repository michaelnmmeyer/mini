#include <lua.h>
#include <lauxlib.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "../mini.h"

/* Begin compatibility code for Lua 5.1. Copy-pasted from Lua's source. */
#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM == 501

static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup+1, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    lua_pushstring(L, l->name);
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -(nup+1));
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup);  /* remove upvalues */
}

#define luaL_newlibtable(L,l) \
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)   (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#endif
/* End compatibility code. */

#define MN_MT "mini"
#define MN_ENC_MT "mini.enc"
#define MN_ITER_MT "mini.iter"

static int mn_lua_enc_new(lua_State *lua)
{
   static const char *const types[] = {
      [MN_STANDARD] = "standard",
      [MN_NUMBERED] = "numbered",
      NULL
   };
   enum mn_type type = luaL_checkoption(lua, 1, "standard", types);

   struct mini_enc **enc = lua_newuserdata(lua, sizeof *enc);
   *enc = mn_enc_new(type);

   luaL_getmetatable(lua, MN_ENC_MT);
   lua_setmetatable(lua, -2);
   return 1;
}

static int mn_lua_enc_add(lua_State *lua)
{
   struct mini_enc **enc = luaL_checkudata(lua, 1, MN_ENC_MT);
   size_t len;
   const void *word = luaL_checklstring(lua, 2, &len);

   int ret = mn_enc_add(*enc, word, len);
   if (ret) {
      /* Programming error. */
      lua_pushstring(lua, mn_strerror(ret));
      return lua_error(lua);
   }
   return 0;
}

static int mn_lua_enc_dump(lua_State *lua)
{
   struct mini_enc **enc = luaL_checkudata(lua, 1, MN_ENC_MT);
   const char *path = luaL_checkstring(lua, 2);

   FILE *fp = fopen(path, "wb");
   if (!fp) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }

   int ret = mn_enc_dump_file(*enc, fp);
   switch (ret) {
   case MN_OK:
      break;
   case MN_EIO:
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   default:
      /* Programming error. */
      lua_pushstring(lua, mn_strerror(ret));
      return lua_error(lua);
   }

   if (fclose(fp)) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }
   lua_pushboolean(lua, 1);
   return 1;
}

static int mn_lua_enc_clear(lua_State *lua)
{
   struct mini_enc **enc = luaL_checkudata(lua, 1, MN_ENC_MT);
   mn_enc_clear(*enc);
   return 0;
}

static int mn_lua_enc_free(lua_State *lua)
{
   struct mini_enc **enc = luaL_checkudata(lua, 1, MN_ENC_MT);
   mn_enc_free(*enc);
   return 0;
}

/* Used in volubile.c. */
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

static uint32_t mn_abs_index(lua_State *lua, int idx, const struct mini *fsa)
{
   int64_t num = luaL_checknumber(lua, idx);
   if (num < 0) {
      num += mn_size(fsa) + 1;
      if (num < 0)
         num = 0;
   }
   return num;
}

static int mn_lua_extract(lua_State *lua)
{
   const struct mini *fsa = check_fsa(lua);
   uint32_t pos = mn_abs_index(lua, 2, fsa);

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
   lua_pushnil(lua);

   struct mini *fsa;
   struct mini_iter *it = mn_lua_iter_new(lua, &fsa);

   uint32_t pos;
   switch (lua_type(lua, 2)) {
   case LUA_TNUMBER: {
      uint32_t num = mn_abs_index(lua, 2, fsa);
      pos = mn_iter_initn(it, fsa, num);
      break;
   }
   case LUA_TSTRING: {
      size_t len;
      const char *str = lua_tolstring(lua, 2, &len);
      const char *mode = luaL_optstring(lua, 3, "string");
      if (!strcmp(mode, "string"))
         pos = mn_iter_inits(it, fsa, str, len);
      else if (!strcmp(mode, "prefix"))
         pos = mn_iter_initp(it, fsa, str, len);
      else
         return luaL_error(lua, "invalid iteration mode: '%s'", mode);
      break;
   }
   case LUA_TNIL:
   case LUA_TNONE:
      pos = mn_iter_init(it, fsa);
      break;
   default: {
      const char *type = lua_typename(lua, lua_type(lua, 2));
      return luaL_error(lua, "bad value at #2 (expect string, number, or nil, have %s)", type);
   }
   }

   lua_pushcclosure(lua, mn_lua_iter_next, 1);
   if (pos)
      lua_pushnumber(lua, pos);
   else
      lua_pushnil(lua);
   return 2;
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
   const luaL_Reg enc_fns[] = {
      {"__gc", mn_lua_enc_free},
      {"add", mn_lua_enc_add},
      {"clear", mn_lua_enc_clear},
      {"dump", mn_lua_enc_dump},
      {NULL, NULL},
   };
   luaL_newmetatable(lua, MN_ENC_MT);
   lua_pushvalue(lua, -1);
   lua_setfield(lua, -2, "__index");
   luaL_setfuncs(lua, enc_fns, 0);

   const luaL_Reg fns[] = {
      {"__gc", mn_lua_free},
      {"__len", mn_lua_size},
      {"locate", mn_lua_locate},
      {"extract", mn_lua_extract},
      {"contains", mn_lua_contains},
      {"type", mn_lua_type},
      {"size", mn_lua_size},
      {"iter", mn_lua_iter_init},
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
      {"encoder", mn_lua_enc_new},
      {"load", mn_lua_load},
      {NULL, NULL},
   };
   luaL_newlib(lua, lib);
   lua_pushnumber(lua, MN_MAX_WORD_LEN);
   lua_setfield(lua, -2, "MAX_WORD_LEN");

   lua_pushstring(lua, MN_VERSION);
   lua_setfield(lua, -2, "VERSION");

   return 1;
}
