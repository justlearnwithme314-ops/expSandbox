#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#ifdef VOXEL_API_BUILD
#define VL_API __declspec(dllexport)
#else
#define VL_API __declspec(dllimport)
#endif
#else
#define VL_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
// Versioned C ABI for offline tools and alternative language frontends.
// One handle must be used by one thread at a time. Strings are UTF-8.
// Each handle owns its own world connection; this is not a live client bridge.
typedef struct vl_game vl_game;
VL_API uint32_t vl_api_version(void);
VL_API vl_game* vl_open(const char* data_root,const char* sqlite_path,int32_t seed,int32_t creative);
VL_API void vl_close(vl_game* game);
VL_API const char* vl_last_error(void);
VL_API int32_t vl_save(vl_game* game);
VL_API int32_t vl_command(vl_game* game,const char* command);
VL_API int32_t vl_density(vl_game* game,float x,float y,float z,float* value);
VL_API int32_t vl_sculpt(vl_game* game,float x,float y,float z,float radius,int32_t add);
VL_API int32_t vl_set_block(vl_game* game,int32_t x,int32_t y,int32_t z,const char* id);
// Returns required size including NUL; zero means failure. Copy into a caller
// owned buffer, or call with NULL/0 first to discover its required size.
VL_API size_t vl_profile_json(vl_game* game,char* buffer,size_t capacity);
#ifdef __cplusplus
}
#endif
