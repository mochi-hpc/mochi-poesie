#ifndef lstraux_h
#define lstraux_h


#include <lua.h>
#include <lauxlib.h>



#ifndef LUAMEMLIB_API
#define LUAMEMLIB_API LUALIB_API
#endif

#ifndef LUAMEMMOD_API
#define LUAMEMMOD_API LUAMOD_API
#endif


#define LUAMEM_TNONE	0
#define LUAMEM_TALLOC	1
#define LUAMEM_TREF	2

#define LUAMEM_ALLOC	"char[]"
#define LUAMEM_REF	"luamem_Ref"

#ifdef __cplusplus
extern "C" {
#endif

LUAMEMLIB_API char *(luamem_newalloc) (lua_State *L, size_t len);

typedef void (*luamem_Unref) (lua_State *L, void *mem, size_t len);

LUAMEMLIB_API void (luamem_newref) (lua_State *L);
LUAMEMLIB_API int (luamem_resetref) (lua_State *L, int idx,
                                     char *mem, size_t len, luamem_Unref unref,
                                     int cleanup);

#define  luamem_setref(L,I,M,S,F) luamem_resetref(L,I,M,S,F,1)

LUAMEMLIB_API int (luamem_type) (lua_State *L, int idx);

#define luamem_ismemory(L,I)	(luamem_type(L,I) != LUAMEM_TNONE)
#define luamem_tomemory(L,I,S)	(luamem_tomemoryx(L,I,S,NULL,NULL))

LUAMEMLIB_API char *(luamem_tomemoryx) (lua_State *L, int idx,
                                        size_t *len, luamem_Unref *unref,
                                        int *type);
LUAMEMLIB_API char *(luamem_checkmemory) (lua_State *L, int idx, size_t *len);


LUAMEMLIB_API int (luamem_isarray) (lua_State *L, int idx);
LUAMEMLIB_API const char *(luamem_toarray) (lua_State *L, int idx, size_t *len);
LUAMEMLIB_API const char *(luamem_asarray) (lua_State *L, int idx, size_t *len);
LUAMEMLIB_API const char *(luamem_checkarray) (lua_State *L, int idx, size_t *len);
LUAMEMLIB_API const char *(luamem_optarray) (lua_State *L, int arg, const char *def, size_t *len);


LUAMEMLIB_API void *(luamem_realloc) (lua_State *L, void *mem, size_t osize,
                                                               size_t nsize);
LUAMEMLIB_API void (luamem_free) (lua_State *L, void *memo, size_t size);
LUAMEMLIB_API size_t (luamem_checklenarg) (lua_State *L, int idx);


#if !defined(MAX_SIZET)
/* maximum value for size_t */
#define MAX_SIZET ((size_t)(~(size_t)0))
#endif

/*
** Some sizes are better limited to fit in 'int', but must also fit in
** 'size_t'. (We assume that 'lua_Integer' cannot be smaller than 'int'.)
*/
#define LUAMEM_MAXSIZE  \
	(sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))


/*
** {======================================================
** Lua stack's buffer support
** =======================================================
*/

LUAMEMLIB_API void (luamem_addvalue) (luaL_Buffer *B);

/* }====================================================== */

#ifdef __cplusplus
}
#endif

#endif
