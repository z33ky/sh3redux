/** @file
 *  Describes the structures necessary to load/read Silent Hill 3 sub-arc files.
 *
 *  @see @ref arc-files
 *
 *  @copyright 2016-2017 Palm Studios and Mike M (<a href="https://twitter.com/perdedork">\@perdedork</a>)
 *
 *  @date 14-12-2016
 *
 *  @author Jesse Buhagiar
 */
#ifndef SH3_SUBARC_HPP_INCLUDED
#define SH3_SUBARC_HPP_INCLUDED

#include <cstdint>
#include <ios>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>

namespace sh3 { namespace arc {
    static constexpr int arcFileNotFound = -1; /**< Status @ref mft::LoadFile() and @ref subarc::LoadFile() return if a file cannot be found. */

    /**
     *  An sub-arc.
     */
    class subarc final
    {
    public:
        /** Index to retrieve a file. */
        using index_t = std::uint16_t;
        /** A mapping of filenames to the file's @ref index_t. */
        using files_map = std::map<std::string, index_t>;

        subarc(subarc&&) = default;
        /** Constructor.
         *  
         *  @param subarcName The name of this @ref subarc.
         *  @param filesMap   The @ref files_map for this @ref subarc.
         */
        subarc(std::string &&subarcName, files_map &&filesMap): path{"data/" + subarcName + ".arc"}, files(std::move(filesMap)) { Reopen(); }

        /**
         *  Open the file again.
         */
        void Reopen();

        /**
         *  Load a file into @c buffer.
         *  
         *  The @c buffer will be resized if necessary.
         *  @c insert is updated to point to the end of the inserted data.
         *  
         *  @param filename Path to the file to load.
         *  @param buffer   The buffer to store the file contents into.
         *  @param insert   An iterator to the insertion position in @c buffer.
         *  
         *  @returns The file length if loading is successful, @ref arcFileNotFound if not.
         */
        int LoadFile(const std::string& filename, std::vector<std::uint8_t>& buffer, std::vector<std::uint8_t>::iterator& insert);

        /**
         *  Load a file into @c buffer.
         *  
         *  The @c buffer will be resized if necessary.
         *  
         *  @param filename Path to the file to load.
         *  @param buffer   The buffer to store the file contents into.
         *  
         *  @returns The file length if loading is successful, @ref arcFileNotFound if not.
         */
        int LoadFile(const std::string& filename, std::vector<std::uint8_t>& buffer) { auto back = end(buffer); return LoadFile(filename, buffer, back); }

        /**
         *  Load a file into @c buffer.
         *  
         *  The @c buffer will be resized if necessary.
         *  @c insert is updated to point to the end of the inserted data.
         *  
         *  @param index  The @ref index_t for the file to load.
         *  @param buffer The buffer to store the file contents into.
         *  @param insert An iterator to the insertion position in @c buffer.
         *  
         *  @returns The file length if loading is successful, @ref arcFileNotFound if not.
         */
        int LoadFile(index_t index, std::vector<std::uint8_t>& buffer, std::vector<std::uint8_t>::iterator& insert);

        /**
         *  Load a file into @c buffer.
         *  
         *  The @c buffer will be resized if necessary.
         *  
         *  @param index  The @ref index_t for the file to load.
         *  @param buffer The buffer to store the file contents into.
         *  
         *  @returns The file length if loading is successful, @ref arcFileNotFound if not.
         */
        int LoadFile(index_t index, std::vector<std::uint8_t>& buffer) { auto back = end(buffer); return LoadFile(index, buffer, back); }

    private:
        std::string path;                          /**< Path to the file. */
        boost::iostreams::mapped_file_source file; /**< The subarc, memory mapped. */

        /** Maps a file (and its associated virtual path) to its subarc index. */
        files_map files;
    };

} }

#endif //SH3_SUBARC_HPP_INCLUDED
