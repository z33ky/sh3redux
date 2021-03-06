/*++

Copyright (c) 2016  Palm Studios

Module Name:
        sh3_window.cpp

Abstract:
        Implementation of sh3_window.hpp

Author:
        Jesse Buhagiar

Environment:

Notes:

Revision History:
        22-12-2016: File Created                                    [jbuhagiar]


--*/
#include "SH3/system/glcontext.hpp"
#include "SH3/system/window.hpp"

sh3_window::sh3_window(int width, int height, const std::string& title)
    : hwnd(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL))
    , context(*this)
{
    context.PrintInfo();
}
