# ============
# == OPENGL ==
# ============
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

# ========
# = GLAD =
# ========
add_library(glad2 STATIC "glad2/src/gl.c")
target_include_directories(glad2 PUBLIC "glad2/include/")

# ==========
# == GLM ===
# ==========
add_subdirectory("glm")

# ==============
# == bitsery ===
# ==============
#add_library(bitsery INTERFACE)
#target_include_directories(bitsery INTERFACE "bitsery/include")
add_subdirectory("bitsery/")

# ==========
# == LUA ===
# ==========
set (LUA_SOURCES 
	"lua/lauxlib.c"
	"lua/lbaselib.c"
	"lua/ltablib.c"
	"lua/lutf8lib.c"

	"lua/lmathlib.c"
	"lua/lstrlib.c"
	"lua/loadlib.c"
	"lua/loslib.c"
	"lua/liolib.c"
	"lua/ldblib.c"
	"lua/lcorolib.c"

	"lua/lapi.c"
	"lua/lstate.c"
	"lua/ldebug.c"
	"lua/lzio.c"
	"lua/llex.c"
	"lua/lvm.c"
	"lua/ldump.c"
	"lua/ltm.c"
	"lua/lstring.c"
	"lua/lopcodes.c"
	"lua/lobject.c"
	"lua/lmem.c"
	"lua/linit.c"
	"lua/ltable.c"
	"lua/lctype.c"
	"lua/ldo.c"
	"lua/lfunc.c"
	"lua/lundump.c"
	"lua/lgc.c"
	"lua/lparser.c"
	"lua/lcode.c"
)
set (LUA_HEADERS 
	"lua/lua.h"
	"lua/lauxlib.h"
	"lua/lualib.h"
)

add_library(lua STATIC ${LUA_SOURCES} ${LUA_HEADERS})
target_include_directories(lua PUBLIC "lua/")


# =========
# = IMGUI =
# =========
file (GLOB IMGUI_SOURCES imgui/**.cpp)
file (GLOB IMGUI_HEADERS imgui/**.h)
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})
target_include_directories(imgui PUBLIC "imgui/")
target_compile_definitions(imgui PUBLIC "IMGUI_USE_STB_SPRINTF")
target_include_directories(imgui PUBLIC "stb/")
target_compile_definitions(imgui PUBLIC "IMGUI_DEFINE_MATH_OPERATORS=1")
target_compile_definitions(imgui PUBLIC "IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")
target_compile_definitions(imgui PUBLIC "IMGUI_DISABLE_OBSOLETE_KEYIO=1")
#target_compile_definitions(imgui PUBLIC "IMGUI_DEBUG_PARANOID")
target_compile_features(imgui PUBLIC cxx_std_17)

# ================
# = IMGUI_STDLIB =
# ================
add_library(imgui_stdlib STATIC "imgui/misc/cpp/imgui_stdlib.cpp" "imgui/misc/cpp/imgui_stdlib.h")
target_include_directories(imgui_stdlib PUBLIC "imgui/misc/cpp")
target_link_libraries(imgui_stdlib PUBLIC imgui)

# ========
# = SDL2 =
# ========
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_LIBC ON CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
add_subdirectory("SDL2/")


# ===================
# = tinifiledialogs = 
# ===================
set(tiny_SRC "tinyfiledialogs/tinyfiledialogs.c")
set(tiny_HEADER "tinyfiledialogs/tinyfiledialogs.h")
add_library(tinyfiledialogs STATIC ${tiny_SRC} ${tiny_HEADER})
target_include_directories(tinyfiledialogs PUBLIC "tinyfiledialogs/")

# ============
# === json ===
# ============
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders ON CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
add_subdirectory("json/")

# =============
# === tracy ===
# =============
set(tracy_SOURCES
	"tracy/public/TracyClient.cpp"
)
add_library(tracy STATIC ${tracy_SOURCES})
target_include_directories(tracy PUBLIC "tracy/public/")
if(OFS_PROFILE)
target_compile_definitions(tracy PUBLIC "TRACY_ENABLE")
endif()

set(SOL2_LUA_VERSION "5.4.4" CACHE BOOL "" FORCE)
set(SOL2_BUILD_LUA OFF CACHE BOOL "" FORCE)
add_subdirectory("sol2/")

# ============
# = refl-cpp =
# ============
add_library(refl-cpp INTERFACE)
target_include_directories(refl-cpp INTERFACE "refl-cpp/include/")

# ===========
# = eventpp =
# ===========
add_library(eventpp INTERFACE)
target_include_directories(eventpp INTERFACE "eventpp/include/")


set (civetweb_SRC
	"civetweb/src/civetweb.c" 
)
set_source_files_properties("civetweb/src/civetweb.c" PROPERTIES LANGUAGE CXX )
add_library(civetweb STATIC ${civetweb_SRC})
target_include_directories(civetweb PUBLIC "civetweb/include/")
target_compile_definitions(civetweb PRIVATE "NO_SSL=1"
											"USE_WEBSOCKET=1")
