
// You only need this 'kit.c' file when you want to build a DLL file out of kit, otherwise just grab the 'kit.h'

// If you want to build 'kit.dll' and 'kit.lib', you can use this C file and run: gcc -shared -o kit.dll kit.c "-Wl,--out-implib,kit.lib" -lgdi32 -luser32 -lwinmm
// To compile a program that uses 'kit.dll' and 'kit.lib', you can run: gcc main.c -o main.exe -L. -lkit -mwindows
//    - This assumes that 'kit.lib' is in the same folder as 'main.c'
//    - '-mwindows' is optional and just prevents a console window from opening
// Note: you have to ship 'kit.dll' along with your executable

#define BUILD_DLL
#define KIT_IMPL
#include "kit.h"