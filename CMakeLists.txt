cmake_minimum_required(VERSION 3.10)
project(Omniscent C)

set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_definitions(-DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
add_compile_options(/Os /Oy /GS- /Gs999999 /MD /GR- /Oi /fp:fast /W3 /Gw /Zl)

set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 0)

set(CRINKLER_EXE "${CMAKE_CURRENT_SOURCE_DIR}/crinkler.exe")

# 定义 Crinkler 参数
set(CRINKLER_FLAGS_LIST
    "/SUBSYSTEM:WINDOWS"
    "/ENTRY:main"
    "/LARGEADDRESSAWARE"
    # "/TINYHEADER"
    # "/TINYIMPORT"
    "/UNSAFEIMPORT"
    "/COMPMODE:SLOW"
    "/ORDERTRIES:5000"
    "/NODEFAULTLIB"
)
# 将列表转换为字符串
string(REPLACE ";" " " CRINKLER_FLAGS_STR "${CRINKLER_FLAGS_LIST}")

# 2. 同样处理库文件
set(CRINKLER_LIBS_LIST
    "kernel32.lib"
    "user32.lib"
    "gdi32.lib"
    "winmm.lib"
    "opengl32.lib"
)
string(REPLACE ";" " " CRINKLER_LIBS_STR "${CRINKLER_LIBS_LIST}")

# 3. 覆盖链接指令
# 注意：对 CRINKLER_EXE 加引号，防止路径中有空格
# <OBJECTS> 是 CMake 自动生成的对象文件列表，它在链接时会自动处理好分隔符
set(CMAKE_C_LINK_EXECUTABLE "\"${CRINKLER_EXE}\" /OUT:<TARGET> ${CRINKLER_FLAGS_STR} <OBJECTS> ${CRINKLER_LIBS_STR}")

add_executable(omniscent WIN32 omniscent_all.c)

add_executable(omniscent_gl WIN32
    src/main.c
    src/omniscent.c
    src/camera.c
    src/model.c
    src/texture.c
    src/palette.c
    src/lightmap.c
    src/midi.c
    src/random.c
    src/renderer_sw.c
    src/renderer_gl.c
    src/renderer_common.c
    src/util.c
)