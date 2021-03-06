/** @file
 *  Window class and related functions
 *
 *  @copyright 2016  Palm Studios
 *
 *  @date 22-12-2016
 *
 *  @author Jesse Buhagiar
 */
#ifndef SH3_WINDOW_HPP_INCLUDED
#define SH3_WINDOW_HPP_INCLUDED

#include <memory>
#include <string>
#include <SDL_video.h>
#include "SH3/system/glcontext.hpp"

/**
 *  Describes a logical Window to interface with SDL2
 */
class sh3_window
{
private:

public:
    sh3_window(int width, int height, const std::string& title);

    std::unique_ptr<SDL_Window, sdl_destroyer> hwnd;        /**< Our window handle */
    sh3_gl::context context;                                /**< This window's OpenGL Context */

private:

};

#endif // SH3_WINDOW_HPP_INCLUDED
