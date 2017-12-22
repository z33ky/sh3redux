#include "SH3/arc/subarc.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "SH3/system/assert.hpp"
#include "SH3/system/log.hpp"

using namespace sh3::arc;

/** @defgroup arc-section-headers arc section header types
 *  @{
 */

namespace {
     /**
      * .arc section header
      */
    struct subarc_header final
    {
        std::uint32_t magic;             /**< File magic number */
        std::uint32_t numFiles;          /**< Number of files located in this sub .arc */
        std::uint32_t dataPointer;       /**< Pointer to the beginning of the data section */
        std::uint32_t unused;            /**< Unused DWORD */
    };

    /**
     *  File entry inside of an .arc section file [*.arc]
     */
    struct subarc_file_entry final
    {
        std::uint32_t offset;            /**< Offset file resides at */
        std::uint32_t fileID;            /**< FileID???? */
        std::uint32_t length;            /**< Length of this file (in bytes) */
        std::uint32_t length2;
    };
}

/** @}*/

void subarc::Reopen()
{
    file.open(path);
    if(!file)
    {
        Log(LogLevel::ERROR, "Failed to open file %s.", path.c_str());
        return;
    }

    static_assert(std::is_standard_layout<subarc_header>::value, "must be castable from char*");
    const subarc_header &header = *reinterpret_cast<const subarc_header*>(file.data());
    static constexpr decltype(header.magic) magic = 0x20030507; /**< Magic number (first 4 bytes) of an subarc header */
    if(header.magic != magic)
    {
        Log(LogLevel::ERROR, "File %s has incorrect header magic.", path.c_str());
        file.close();
        return;
    }
}

int subarc::LoadFile(const std::string& filename, std::vector<std::uint8_t>& buffer, std::vector<std::uint8_t>::iterator& insert)
{
    using std::next;

    auto matching = files.equal_range(filename);
    if(matching.first == matching.second)
    {
        return arcFileNotFound;
    }

    using std::next;
    // files.first is the std::map iterator
    auto match = matching.first;
    if(next(match) != matching.second)
    {
        Log(LogLevel::WARN, "Multiple files with name %s exist in %s.", filename.c_str(), path.c_str());
    }
    // match->second is the value of the entry the iterator is pointing at
    return LoadFile(match->second, buffer, insert);
}

int subarc::LoadFile(index_t index, std::vector<std::uint8_t>& buffer, std::vector<std::uint8_t>::iterator& insert)
{
    using std::end;
    using std::prev;

    if(!file)
    {
        return arcFileNotFound;
    }

    // Seek to the file entry and read it
    static_assert(std::is_standard_layout<subarc_file_entry>::value, "must be castable from char*");
    ASSERT(file.size() >= sizeof(subarc_header) + (index + 1) * sizeof(subarc_file_entry));
    const subarc_file_entry &fileEntry = *reinterpret_cast<const subarc_file_entry*>(file.data() + sizeof(subarc_header) + index * sizeof(fileEntry));
    ASSERT(file.size() >= fileEntry.offset + fileEntry.length);
    const auto dataBegin = file.begin() + fileEntry.offset,
               dataEnd   = dataBegin + fileEntry.length;

    auto space = distance(insert, end(buffer));
    ASSERT(space >= 0);
    if(space < fileEntry.length)
    {
        using size_type = std::remove_reference<decltype(buffer)>::type::size_type;
        buffer.resize(fileEntry.length - static_cast<size_type>(space));
        insert = prev(end(buffer), fileEntry.length);
    }

    std::copy(dataBegin, dataEnd, insert);
    advance(insert, fileEntry.offset);

    //FIXME: use error_code pattern like mft_reader::ReadFile
    return static_cast<int>(fileEntry.length);
}
