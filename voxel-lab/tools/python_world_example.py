"""Use the C++ world core from Python via ctypes. Creates its own SQLite world."""
import ctypes as c
import json
import sys
from pathlib import Path

root=Path(__file__).resolve().parents[1]
default_lib=root/"voxel-api.dll" if (root/"voxel-api.dll").exists() else root/"build/native/Release/voxel-api.dll"
lib=Path(sys.argv[1]) if len(sys.argv)>1 else default_lib
api=c.CDLL(str(lib.resolve()))
api.vl_api_version.restype=c.c_uint32
api.vl_open.argtypes=[c.c_char_p,c.c_char_p,c.c_int32,c.c_int32]
api.vl_open.restype=c.c_void_p
api.vl_close.argtypes=[c.c_void_p]
api.vl_save.argtypes=[c.c_void_p]
api.vl_command.argtypes=[c.c_void_p,c.c_char_p]
api.vl_density.argtypes=[c.c_void_p,c.c_float,c.c_float,c.c_float,c.POINTER(c.c_float)]
api.vl_sculpt.argtypes=[c.c_void_p,c.c_float,c.c_float,c.c_float,c.c_float,c.c_int32]
api.vl_profile_json.argtypes=[c.c_void_p,c.c_void_p,c.c_size_t]
api.vl_profile_json.restype=c.c_size_t
api.vl_last_error.restype=c.c_char_p

def ensure(ok):
    if not ok: raise RuntimeError(api.vl_last_error().decode())

assert api.vl_api_version()==1
path=root/"test-output/python-tool.sqlite"
path.parent.mkdir(exist_ok=True)
game=api.vl_open(str(root).encode(),str(path).encode(),8128,1)
ensure(game)
try:
    density=c.c_float()
    ensure(api.vl_density(game,4,12,4,c.byref(density)))
    print("Density before edit:",density.value)
    ensure(api.vl_sculpt(game,4,12,4,1.5,0))
    ensure(api.vl_command(game,b"/give core:log 4"))
    length=api.vl_profile_json(game,None,0)
    buffer=c.create_string_buffer(length)
    ensure(api.vl_profile_json(game,buffer,length))
    profile=json.loads(buffer.value)
    assert profile["inventory"]
    ensure(api.vl_save(game))
    # Errors are contained at the C boundary; caller-owned buffers and handles
    # never expose STL types or C++ exceptions to Python.
    assert api.vl_density(game,float("nan"),0,0,c.byref(density))==0
    assert api.vl_sculpt(game,0,0,0,100,0)==0
    assert api.vl_profile_json(None,None,0)==0
    print("Python called the C++ core and saved",path)
finally:
    api.vl_close(game)

unicode_path=root/"test-output"/"world-\u65e5\u672c.sqlite"
game=api.vl_open(str(root).encode(),str(unicode_path).encode(),8128,1)
ensure(game)
try:
    ensure(api.vl_save(game))
    assert unicode_path.exists()
finally:
    api.vl_close(game)
print("C ABI error handling and UTF-8 world paths passed")
