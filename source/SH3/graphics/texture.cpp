/** @file
 *
 *  Implementation of texture.hpp
 *
 *  @copyright 2016  Palm Studios
 *
 *  @date 2-1-2017
 *
 *  @author Jesse Buhagiar
 */
#include <SH3/graphics/texture.hpp>
#include <SH3/system/assert.hpp>
#include <SH3/system/log.hpp>
#include <SH3/arc/mft.hpp>
#include <SH3/arc/vfile.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <functional>
#include <limits>

using namespace sh3_graphics;

#pragma pack(push, 1)
struct tga_header
{
    static constexpr std::uint8_t TYPE_RGB24 = 2;
    static constexpr std::uint8_t FLAGS_FLIP = 0x20;

    std::uint8_t id_size = 0;               /**< Size of the ID field that follows this header (usually 0) */
    std::uint8_t colormap = 0;              /**< Is this image paletted (THIS MUST ALWAYS BE 0 FOR US!) */
    std::uint8_t type = TYPE_RGB24;         /**< Type of image (ALWAYS 2 FOR RGB24!) */
    std::uint8_t unused[5] = {};

    std::uint16_t x_origin = 0;             /**< Co-ordinate for first X value */
    std::uint16_t y_origin = 0;             /**< Co-ordinate for first Y value */
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t bpp = 24;                  /**< Bits per pixel */
    std::uint8_t flags = FLAGS_FLIP;
};
#pragma pack(pop)

namespace
{
/**
 *  Dump a texture to a TARGA/TGA file
 *
 *  @tparam DataRange The type for @p data.
 *
 *  @param width  The Width of this texture.
 *  @param height The Height of this texture.
 *  @param data   Pixel data.
 *  @param bpp    Bitness of data (not the actual texture).
 */
//FIXME: this might introduce code-bloat. If this is the case we can use boost::iterator_range<std::uint8_t*> instead of a template.
template<typename DataRange>
void DumpRGB2Bitmap(std::uint32_t width, std::uint32_t height, const DataRange& data, std::uint8_t bpp)
{
    tga_header header;
    std::ofstream file("output.tga", std::ios::binary); // Open the stream for binary output

    ASSERT(width <= std::numeric_limits<decltype(header.width)>::max());
    ASSERT(height <= std::numeric_limits<decltype(header.height)>::max());

    header.width        = static_cast<std::uint16_t>(width);
    header.height       = static_cast<std::uint16_t>(height);

    switch(bpp)
    {
    case 16:
        header.bpp = 16;
        break;
    case 24:
        header.bpp = 24;
        break;
    case 32:
        header.bpp = 32;
        break;
    default:
        Log(LogLevel::WARN, "DumpRGB2Bitmap( ): Warning: Invalid bpp passed to function. Not writing image data.");
        return;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    assert(data.size() <= std::numeric_limits<std::streamsize>::max() / sizeof(data.front()));
    file.write(reinterpret_cast<const char*>(&data.front()), static_cast<std::streamsize>(data.size() * sizeof(data.front())));
}
}

//TODO: Scale the texture and then
void sh3_texture::Load(sh3::arc::mft& mft, const std::string& filename)
{
    pretexture_rc preheader;
    sh3::arc::mft::load_error e; //FIXME
    preheader.LoadFromFile(mft, filename.c_str(), e);
    sh3_texture_header::resource header_rc;
    if(preheader.Check())
    {
        header_rc = preheader.SubHeader<texture_rc::header>();
    }
    else
    {
        //if we have no preheader, we likely just have the texture itself
        header_rc.LoadFromFile(mft, filename.c_str(), e);
        /*if(e)
        {
            die("");
        }*/
    }

    if(!header_rc.Check())
    {
        Log(LogLevel::ERROR, "Tried to load texture %s, but header-check failed.", filename.c_str());
        //FIXME
        die("unimplemented: missing texture fallback");
    }

    auto header = header_rc.Header();

    //FIXME: switch
    if(header.GetRealBPP() == sh3_texture::PixelFormat::PALETTE)
    {
        //TODO: perhaps header_rc.Data(sh3_texture_header::DataKind::PALETTE)?
        std::vector<rgba> palette = header_rc.SubHeader<palette_info>().Data();
        ASSERT(palette.size() <= std::numeric_limits<std::uint8_t>::max() + 1);

        // Now that we've completely loaded the palette in its entirety, we can get the 8-bit index value
        // from the data section of the file and get it's color in the palette!

        //===---THIS IS A CLUSTER FUCK FOR NOW UNTIL WE UNDERSTAND HOW IN THE NAME OF CHRIST THIS WORKS---===//
        std::vector<uint8_t> data;
        data.reserve(static_cast<std::size_t>(header.texWidth * header.texHeight) * 3u); // We strip the Alpha channel from the BGRA pixel beacuse it is hard locked to 0x80 (not 0xFF!!)

        if(header.texWidth > 96) // Apparently this is the distortion flag?!?!
        {
            std::vector<std::uint8_t> iBuffer(header.texSize);  // Our index buffer that we put transformed indices into

            if(header.texWidth % 16u != 0)
            {
                Log(LogLevel::WARN, "sh3_texture::Load( ): Warning: texWidth not divisible by 16!");
                header.texWidth = static_cast<decltype(header.texWidth)>(header.texWidth - header.texWidth % 16u);
            }

            if(header.texHeight % 4u != 0)
            {
                Log(LogLevel::WARN, "sh3_texture::Load ( ): Warning: texHeight not divisible by 4!");
                header.texHeight = static_cast<decltype(header.texHeight)>(header.texHeight - header.texHeight % 4u);
            }

            std::uint32_t x = 0;
            std::uint32_t y = 0;
            bool offsetFlipper = false;

            //TODO: implement header_rc.Data(sh3_texture_header::DataKind::INDEX) instead
            auto it = begin(header_rc.RawData());
            // We need to step through each index and unencrypt it
            // FIXME: distortion on 16-pixel wide block on the left
            while(true)
            {
                for(unsigned i = 0; i < 32; ++i)
                {
                    std::uint8_t index = *(it++);

                    auto xoffset = static_cast<std::uint8_t>(((i << 2) & 0xfu) + ((i >> 2) & 0xfu));
                    if(i > 16 && i % 2u) // aka (i & 17) == 17
                    {
                        xoffset ^= 8u;
                        xoffset &= 0xfu;
                    }
                    if(offsetFlipper)
                    {
                        xoffset ^= 4u;
                    }

                    const auto tempx = x + xoffset; // - 16;
                    // every other pixel is for (y + 2)
                    const auto tempy = y + ((i % 2u) ? 2u : 0u);
                    const auto idx =  (header.texWidth * tempy) + tempx % header.texWidth;
                    ASSERT(idx <= iBuffer.size());
                    iBuffer[idx] = index;
                }

                x += 16;
                if(x < header.texWidth)
                {
                    continue;
                }

                x = 0;

                ++y;
                if(y % 2 == 0)
                {
                    // each iteration we read two lines: offset 0 and 2
                    // so after two iterations, we have read the lines with offset 0 and 2, 1 and 3
                    // so now we need to skip two additional lines, since we read 2 and 3 already
                    y += 2;

                    if(y >= header.texHeight)
                    {
                        Log(LogLevel::WARN, "sh3_texture::Load( ): Warning: y >= header.texHeight!");
                        break;
                    }

                    offsetFlipper = !offsetFlipper;
                }

                if(y == header.texHeight)
                {
                    break;
                }
            }
            ASSERT(it == end(header_rc.RawData()));

            // Christ help me...
            for(std::uint8_t index : iBuffer)
            {
                ASSERT(index <= palette.size());
                rgba pixel = palette[index];

                data.push_back(pixel.r);
                data.push_back(pixel.g);
                data.push_back(pixel.b);
            }
        }
        else // If the distortion flag isn't set, just read the pixel data in from the palette.
        {
            //TODO: implement header_rc.Data(sh3_texture_header::DataKind::INDEX) instead
            for(std::uint8_t index : header_rc.RawData())
            {
                ASSERT(palette.size() <= index);
                rgba pixel = palette[index];

                data.push_back(pixel.r);
                data.push_back(pixel.g);
                data.push_back(pixel.b);
            }
        }

        DumpRGB2Bitmap(header.texWidth, header.texHeight, data, 24);
        UploadTexture(data.data(), header.texWidth, header.texHeight, header.bpp);
        return;
    }
    else if(header.GetRealBPP() == sh3_texture::PixelFormat::RGBA)
    {
        DumpRGB2Bitmap(header.texWidth, header.texHeight, header_rc.RawData(), 32);
    }
    else if(header.GetRealBPP() == sh3_texture::PixelFormat::BGR)
    {
        DumpRGB2Bitmap(header.texWidth, header.texHeight, header_rc.RawData(), 24); // Output will be reversed!
    }
    else if(header.GetRealBPP() == sh3_texture::PixelFormat::RGBA16)
    {
        //TODO: Some kind of fucked up shit here. I think this is R5G5B5A1 or something like that..
        DumpRGB2Bitmap(header.texWidth, header.texHeight, header_rc.RawData(), 16);
    }
    else
    {
        die("sh3_texture::Load( ): Unknown Pixel Format, %d", header.GetRealBPP());
    }

    UploadTexture(&header_rc.RawData().front(), header.texWidth, header.texHeight, header.GetRealBPP());
}

void sh3_texture::UploadTexture(const std::uint8_t *data, std::uint16_t texWidth, std::uint16_t texHeight, std::uint8_t bpp)
{
    glGenTextures(1, &tex);             // Create a texture
    glBindTexture(GL_TEXTURE_2D, tex);  // Bind it for use

    GLenum srcFormat;
    GLint dstFormat;
    GLenum type;

    // Create the texture according to its pixel format!
    switch(bpp)
    {
        case sh3_texture::PixelFormat::RGBA:     // Regular 32-bit RGBA
            srcFormat = GL_RGBA;
            dstFormat = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            break;
        case sh3_texture::PixelFormat::BGR:      // 24-bit BGR
            srcFormat = GL_BGR;
            dstFormat = GL_RGB;
            type = GL_UNSIGNED_BYTE;
            break;
        case sh3_texture::PixelFormat::RGBA16:   // 16-bit RGBA. OpenGL supports this (I think)
            srcFormat = GL_RGBA;
            dstFormat = GL_RGBA;
            type = GL_UNSIGNED_SHORT_5_5_5_1;
            break;
        case sh3_texture::PixelFormat::PALETTE:
            srcFormat = GL_RGB;
            dstFormat = GL_RGB;
            type = GL_UNSIGNED_BYTE;
            break;
        default:
            die("sh3_texture::Load( ): Invalid pixel format: %d", bpp);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, dstFormat, texWidth, texHeight, 0, srcFormat, type, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Use linear interpolation for the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, tex); // Un-bind this texture.
}

void sh3_texture::Bind(GLenum textureUnit)
{
    ASSERT(textureUnit >= GL_TEXTURE0 && textureUnit <= GL_TEXTURE31);

    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, tex);
}

void sh3_texture::Unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}
