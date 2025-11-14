// header.h: 标准系统包含文件的包含文件，
// 或特定于项目的包含文件
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

// GDI+ 等头文件需要 IStream/PROPID/byte 等 COM 类型。
// 因为启用了 WIN32_LEAN_AND_MEAN，某些较少用到的声明被排除了，显式包含以补回这些类型。
#include <objidl.h>

// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
