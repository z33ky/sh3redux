/** @file
 *
 *  Functions and structures to load a texture from an arc section
 *
 *  @copyright 2016  Palm Studios, Mike Meinhardt and de_lof
 *
 *  @note   I'd like to thank Mike for all this, he put a lot of time into reverse engineering
 *          all of the file types, including all of the textures, meshes and even the motion capture
 *          skeletal animation that Konami captured. Thanks mate!
 *
 *  @note It would seem the 8-bit texture palette comes at the END of the texture, not at beginning like one would expect.
 *  @note bpp == 32, RGBA; bpp == 24, BGR; bpp == 16, RGBA16; bpp=8, Paletted.
 *
 *  @date 2-1-2017
 *
 *  @author Jesse Buhagiar
 */
#ifndef SH3_TEXTURE_HPP_INCLUDED
#define SH3_TEXTURE_HPP_INCLUDED

#include "SH3/arc/vfile.hpp"
#include "SH3/arc/resource.hpp"
#include "SH3/system/log.hpp"

#include <boost/range/adaptor/strided.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <GL/glew.h>
#include <GL/gl.h>

namespace sh3 { namespace arc {
    struct mft;
} }

namespace sh3_graphics
{
    /** @defgroup graphics-headers Graphics headers
     *  @{
     */

    #pragma pack(push, 1)

    struct rgba
    {
        std::uint8_t r, g, b, a;
    };

    struct palette_info_resource;
    /**
     *  Color palette structure. Contains information about the colour palette.
     */
    struct palette_info : public sh3::arc::resource_base<palette_info, palette_info_resource>
    {
        std::uint32_t paletteSize;      /**< Number of colors (??) in our color palette */
        std::uint32_t unused1[2];       /**< Unused as far as I can tell */
        std::uint8_t  bytes_per_pixel;  /**< Number of bytes per pixel */
        std::uint8_t  unused2;          /**< Blank Byte */
        std::uint8_t  entrySize;        /**< Size of one color block in this palette */
        std::uint8_t  unknown[17];      /**< Unknown or unused bytes */
        std::uint8_t  distortion;       /**< I have no clue what this is, but it could be important */
        std::uint8_t  pad[15];          /**< These are all zero. Here so we can align to the palette after a read */

        //dummy
        struct load_result {};
        static void Load(std::vector<std::uint8_t> &, sh3::arc::mft&, const std::string&, load_result&) {}

        constexpr bool Check() const { return true; }

        // Palette information is stored in blocks (usually of size 64-bytes). We also know how large the
        // palette is (in bytes, including padding between blocks). From this, we can deduce (with a bit of math)
        // that the whole palette occupies ~ paletteSize/entrySize bytes of space, contains a total of
        // entrySize/bypp colors per block, which therefore means we have a total of nBlocks * col_per_block colors,
        // which equates to about 256-colors in total (which seems accurate for an 8-bit texture).
        constexpr std::size_t GetBlockCount() const { return (paletteSize / entrySize) / bytes_per_pixel; }
        constexpr std::size_t GetColorsPerBlock() const { return entrySize / bytes_per_pixel; }

        //data is chunked
        //each chunk is 256 bytes, though only GetColorPerBlock() are the palette colors
        constexpr std::size_t DataSize(std::size_t maxSize, std::size_t idx) const;
        constexpr std::size_t DataOffset(std::size_t idx) const { return sizeof(*this) + 256 * idx; }
    };

    /**
     *  Color palette resource.
     */
    struct palette_info_resource : public sh3::arc::resource<palette_info>
    {
    public:
        using resource::resource;

        using resource::Data;

        std::vector<rgba> Data() { return static_cast<const palette_info_resource&>(*this).Data(); }
        std::vector<rgba> Data() const
        {
            using std::advance;
            using std::begin;
            using std::distance;
            using std::end;
            using std::next;

            std::vector<rgba> palette;
            const auto &header = Header();
            palette.reserve(header.GetColorsPerBlock() * header.GetBlockCount());
            for(std::size_t i = 0; i < header.GetBlockCount(); ++i)
            {
                auto block = RawData(i)
                           //treat as rgba, not bytes
                           | boost::adaptors::strided(sizeof(rgba))
                           | boost::adaptors::transformed([](const std::uint8_t &x){ return reinterpret_cast<const rgba&>(x); });
                ASSERT(block.size() == header.GetColorsPerBlock());
                palette.insert(end(palette), begin(block), end(block));
            }

            static constexpr std::size_t swapDistance = 32; // distance between swaps
            static constexpr std::size_t swapSize = 8; // amount of colors to swap
            // swap colors every 32 pixels, starting from the 8th
            if(palette.size() > 8)
            {
                for(auto iter = next(begin(palette), 8); static_cast<std::size_t>(distance(iter, end(palette))) > swapDistance; advance(iter, swapDistance))
                {
                    // swap 8 colors
                    const auto swapBlock = next(iter, swapSize);
                    if(static_cast<std::size_t>(distance(swapBlock, end(palette))) < swapSize)
                    {
                        Log(LogLevel::WARN, "Palette doesn't have enough colors left for swapping.");
                        break;
                    }
                    std::swap_ranges(iter, swapBlock, swapBlock);
                }
            }

            //return std::move(palette); //pessimizing
            return palette;
        }

        //this isn't implemented because it's missing color swapping and possibly be less efficient than caching (as Data() does) if it were implemented
        //const rgba& operator[](std::size_t idx) const { return *(reinterpret_cast<rgba*>(RawData(idx / colorsPerBlock)) + (idx % colorsPerBlock)); }
    };
    constexpr std::size_t palette_info::DataSize(std::size_t maxSize, std::size_t idx) const { static_cast<void>(maxSize); static_cast<void>(idx); return GetColorsPerBlock() * sizeof(decltype(std::declval<palette_info_resource>().Data().front())); }

    /**
     *  Header that comes after the batch header. Contains information about the texture.
     */
    struct sh3_texture_info_header //: public sh3::arc::resource_base<sh3_texture_info_header>
    {
        std::uint32_t texHeaderMarker;      /**< Magic Number. This is ALWAYS 0xFFFFFFFF */
        std::uint32_t unused1;               /**< Unused 32-bit value. Apparently for format identification. */
        std::uint16_t width;                /**< The hidth of this texture */
        std::uint16_t height;               /**< The height of this texture */
        std::uint8_t  bpp;                  /**< Number of bits per pixel of this texture. NOTE: 8bpp is paletted! */
        std::uint8_t  dataOffset;           /**< #bytes from texHeaderSize+16 to 128 (0 filled) */
        std::uint16_t padding;              /**< Possibly padding, as it's usually 0 */
        std::uint32_t texSize;              /**< The size of this texture in bytes (w * h * [bpp/8]) */
        std::uint32_t texFileSize;          /**< = texSize + texHeaderSize + 16 + endFilleSize */
        std::uint32_t unknown;              /**< This is unknown/unused */
        std::uint8_t  widthattrib;          /**< 256 - 8; 32 - 5; 1024 - A; 512 - 9 (I have no clue what this is...) */
        std::uint8_t  heightattrib;         /**< Same deal */
        std::uint16_t magic;                /**< Always 0x9999 */
    };

    /**
     * Full texture header.
     *
     * Both batch and individual texture.
     */
    struct sh3_texture_header : public sh3::arc::resource_base<sh3_texture_header>
    {
        friend typename resource_base::resource;

        std::uint32_t batchHeaderMarker;    /**< This should be 0xFFFFFFFF to mark a header chunk */
        std::uint8_t  unused1[4];           /**< There are a lot of unused DWORDs, I assume to align everything nicely */
        std::uint32_t batchHeaderSize;      /**< Size of the first part of the whole header */
        std::uint32_t batchSize;            /**< = res * res * (bpp/8) * #tex + 128 * #tex */
        std::uint8_t  unused2[4];           /**< Unused **/
        std::uint32_t numBatchedTextures;   /**< Number of textures in this texture file */
        std::uint8_t  unused3[8];           /**< Unused **/
        std::uint32_t texHeaderSegMarker;   /**< Secondary texture marker. This signifies the start of the texture information header. This is also 0xFFFFFFFF */
        std::uint8_t  unused4[4];           /**< Unused **/
        std::uint16_t texWidth;             /**< The width of this texture */
        std::uint16_t texHeight;            /**< The height of this texture */
        std::uint8_t  bpp;                  /**< Number of bits per pixel of this texture. NOTE: 8bpp is believed to be paletted! */
        std::uint8_t  dataOffset;           /**< #bytes from texHeaderSize+16 to 128 (0 filled) */
        std::uint8_t  unused5[2];           /**< Unused **/
        std::uint32_t texSize;              /**< The size of this texture in bytes (w * h * [bpp/8]) */
        std::uint32_t texFileSize;          /**< = texSize + texHeaderSize + 16 + endFilleSize */
        std::uint8_t  unused6[4];           /**< Unused **/
        std::uint32_t unknown1;             /**< Completely unknown, probably unimportant for now */
        std::uint32_t unknown2;             /**< Usually 1! */
        std::uint32_t unused7[15];          /**< Unused **/

        //dummy
        struct load_result {};
        static void Load(std::vector<std::uint8_t> &, sh3::arc::mft&, const std::string&, load_result&) {}

        //I'm not sure if this is required
        constexpr std::size_t DataOffset() const { return GetRealBPP() == 8 /*PixelFormat::PALETTE*/ ? texFileSize - texSize : sizeof(*this); }

        constexpr bool Check() const
        {
            return texSize == static_cast<decltype(texSize)>(texWidth * texHeight * GetRealBPP()) / 8u;
        }
        //FIXME: return PixelFormat
        constexpr std::uint8_t GetRealBPP() const { return texSize == static_cast<decltype(texSize)>(texWidth * texHeight) * 4u ? 32 : bpp; }
        std::size_t DataSize(std::size_t maxSize) const { static_cast<void>(maxSize); return texSize; }

        //void Prepare() { if(texSize == static_cast<decltype(texSize)>(texWidth * texHeight) * 4u) { bpp = 32; } }

    private:
        template<typename sub_header>
        constexpr std::size_t SubHeader() const;
    };

    template<>
    constexpr std::size_t sh3_texture_header::SubHeader<palette_info>() const
        { /*ASSERT(IsPaletted());*/ return batchHeaderSize + texFileSize; }

    using texture_rc = sh3::arc::resource<sh3_texture_header>;

    struct sh3_texture_preheader : public sh3::arc::resource_base<sh3_texture_preheader>
    {
        friend struct resource_base;
        friend typename resource_base::resource;

    public:
        std::uint32_t zero;
        std::uint8_t unknown0[8];
        std::uint32_t magic;
        static constexpr std::uint64_t magicNumber = 0xA7A7A7A7;
        std::uint8_t unknown1[48];

        //dummy
        struct load_result {};
        static void Load(std::vector<std::uint8_t> &, sh3::arc::mft&, const std::string&, load_result&) {}

        constexpr bool Check() const { return zero == 0 && magic == magicNumber; }
        constexpr std::size_t Size() const { return sizeof(*this) + sizeof(sh3_texture_header); }

        //constexpr std::size_t DataOffset() = delete; //{ static_assert(false, "No data 4 u"); return 0; }
        constexpr std::size_t DataOffset() const;

    private:
        template<typename sub_header>
        constexpr std::size_t SubHeader() const;
    };

    template<>
    constexpr std::size_t sh3_texture_preheader::SubHeader<sh3_texture_header>() const
    { return sizeof(*this); }

    constexpr std::size_t sh3_texture_preheader::DataOffset() const { return sizeof(*this) + SubHeader<sh3_texture_header>(); }

    using pretexture_rc = sh3::arc::resource<sh3_texture_preheader>;

    #pragma pack(pop)

    /**@}*/

    /**
     *
     * Describes a logical texture that can be bound to OpenGL
     *
     * Defines a few bits of data and some functions to load in a texture
     * from a SILENT HILL 3 .arc section.both batch and individual texture
     *
     */
    struct sh3_texture final
    {
    public:

        enum PixelFormat
        {
            RGBA    = 32,
            BGR     = 24,
            RGBA16  = 16,
            PALETTE = 8,
        };

        sh3_texture(sh3::arc::mft& mft, const std::string& filename){Load(mft, filename);}
        ~sh3_texture(){}

        /**
         *  Loads a texture from a Virtual File and creates a logical texture
         *  on the gpu
         *
         *  @note Should we scale this ala SILENT HILL 3's "Interal Render Resolution"???
         */
        void Load(sh3::arc::mft& mft, const std::string& filename);

        /**
         *  Bind this texture for use with any draw calls
         *
         *  @param textureUnit The texture unit we want to bind this texture to
         */
        void Bind(GLenum textureUnit);

         /**
          * Unbind this texture from the
          */
         void Unbind();

    private:
         /**
          *  TODO
          */
         void UploadTexture(const std::uint8_t *data, std::uint16_t texWidth, std::uint16_t texHeight, std::uint8_t bpp);

    private:
        GLuint tex;                         /**< ID representing this texture */
    };
}

#endif // SH3_TEXTURE_HPP_INCLUDED
