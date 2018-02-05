/** @file
 *  Definition of generic resource types.
 *  
 *  The resource types help handling byte arrays as data with headers.
 *  Resources may consist of multiple data packs (e.g. individual sprites in a sprite sheet) and sub-resources (e.g. a texture containing palette data, which itself is a resource).
 *  
 *  This types allow abstracting over how the data is loaded and managed.
 *  Currently, the resources have an internal @c std::vector, though we can experiment with memory-mapping or manually managing an heap-allocated array to avoid having to copy data for a sub-resource.
 *  A consequence of this is that references to the data is always const.
 */
#ifndef SH3_ARC_RESOURCE_HPP_INCLUDED
#define SH3_ARC_RESOURCE_HPP_INCLUDED

#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#include <boost/range/iterator_range_core.hpp>

#include "SH3/arc/mft.hpp"
#include "SH3/system/assert.hpp"

namespace sh3 { namespace arc {
    template<typename header>
    struct resource;

    //TODO: more examples
#if 0
    struct __attribute((packed)) my_header : public resource_base<my_header>
    {
        std::uint16_t magic;
        std::uint64_t dataSize;

    protcted:
        bool Check() const { return magic == 0x76543210; }
        std::size_t DataSize() const { return dataSize; }
    };

    struct __attribute((packed)) my_sub_header : public resource_base<my_sub_header>
    {
        std::uint64_t dataOffset; //offset to data after this header in bytes

    protcted:
        constexpr bool Check() const { return true; }
        constexpr std::size_t DataSize() const { return 0x40; } //fixed size data
        std::size_t DataOffset() const { return dataOffset; }
    };
#endif

    /**
     *  Base class for resource headers.
     *  
     *  This struct provides some default implementations for convenience of implementing resource headers.
     *  
     *  @tparam header        The resource header type that inherits this.
     *  @tparam resource_type The resource type to use for this header.
     */
    template<typename header, typename resource_type = resource<header>>
    struct resource_base
    {
        friend struct resource<header>;
        //static_assert(std::is_standard_layout<header>::value, "The header must be standard layout");
        //static_assert(sizeof(resource_base) == 0, "resource_base cannot contain any member variables, since we alias the resource memory");

    public:
        using resource = resource_type; ///< A @ref resource of this.

        //It would be kinda pretty to have the header being the resource,
        //but for one we can't implement IsLoaded() since `this` must not be nullptr. And neither RawData() (at least not with a range) or AssertSize().
        #if 0
        template<typename T, typename... Args>
        const T& SubHeader(Args &&...args) const;

        //FIXME
        //bool IsLoaded() const { if(raw.empty()) return false; AssertSize(sizeof(*this)); return true; }
        bool IsLoaded() const { return this != nullptr; } //undefined behavior

    protected:
        void AssertSize(std::size_t size) const { ASSERT(sizeof(*this) >= size); }

        template<typename T>
        static const T& Convert(const T& thing) { return thing; }

        template<typename T>
        const T& Convert(std::size_t offset) const;

        #if 0 //unsure if needed
        template<typename T>
        T Convert(const std::string &filename) const;
        #endif
        #endif

    protected:
        /**
         *  The offset from the beginning to the data in bytes.
         *  
         *  Headers can implement this function with arguments as well.
         */
        constexpr std::size_t DataOffset() const { return sizeof(header); }
        /**
         *  The size of the data in bytes.
         *  
         *  @param len The length of the complete data.
         *  
         *  Like @ref DataOffset, this function can be implemented with arguments as well.
         */
        //FIXME: we might need to revert to template fuckery for this, since this is problematic if header implements DataOffset(size_t), but not DataSize(size_t)
        constexpr std::size_t DataSize(std::size_t len) const { return len; }

        /**
         *  The offset from the beginning to the header in bytes.
         */
        static constexpr std::size_t HeaderOffset() { return 0; }
    };

    /**
     *  A resource accessor.
     *  
     *  This struct allows convenient access to a resource's data using its header definition (see @ref resource_base).
     *  
     *  Access to the data is const to allow using memory mapping.
     *  To have modifiable data, a copy to local memory must be made.
     *  
     *  @tparam header_type The header for the resource.
     *  
     */
    template<typename header_type>
    struct resource
    {
        static_assert(std::is_standard_layout<header_type>::value, "The header must be standard layout");

    protected:
        //note: This would become std::uint8_t* const when we change to memory mapped files
        using raw_storage = std::vector<std::uint8_t>; ///< The type for the data forming the resource.
        using const_iterator_range = boost::iterator_range<raw_storage::const_iterator>; ///< An view on the data.

    public:
        using header = header_type; ///< The header type for this resource.

    public:
        /**
         *  Constructor.
         */
        resource() { /*raw.reserve(sizeof(header));*/ /*sizeof(header) + header::SizeHint()*/ }
        /**
         *  Constructor.
         *  
         *  @param data The data of the resource.
         */
        resource(raw_storage&& data): raw{std::move(data)} {}
        /**
         *  Move-Constructor.
         */
        resource(resource&&) = default;
        /**
         *  Destructor.
         */
        ~resource() = default;

        resource& operator=(const resource&) = default;
        resource& operator=(resource&&) = default;

        /**
         *  Load resource by reading from file.
         *  
         *  The resource must not be loaded yet.
         *  
         *  @param mft
         *  @param filename
         *  @param e
         */
        //FIXME: partial read -> e == true; check return.
        void LoadFromFile(mft& mft, const std::string& filename, mft::load_error& e) { ASSERT(!IsLoaded()); mft.LoadFile(filename, raw, e); if(IsLoaded() && !Header().Check()) { raw.clear(); /*FIXME: error*/ } }

        /**
         *  Get the header for this resource.
         *  
         *  The resource must have been successfully loaded.
         *  
         *  @returns The header.
         */
        //FIXME: can we have multiple headers?
        //TODO: an option<const header&> variant that is none if !IsLoaded()?
        const header& Header() const { using std::cbegin; using std::next; ASSERT(IsLoaded()); return reinterpret_cast<const header&>(*next(cbegin(raw), header::HeaderOffset())); }

        /**
         *  Obtain a sub-header.
         *  
         *  @tparam T    The type of the sub-header.
         *  @tparam Args The type for the arguments to identify the subheader.
         *  
         *  @param args The arguments to identify the subheader.
         *  
         *  @returns A @c T::resource (usually a @ref resource).
         */
        template<typename T, typename... Args>
        typename T::resource SubHeader(Args &&...args) const { ASSERT(IsLoaded()); return Convert<T>(Header().template SubHeader<T>(std::forward<Args>(args)...)); }

        /**
         *  Obtain the data as bytes.
         *  
         *  @tparam Args The types for the arguments to identify the data.
         *  
         *  @param args The arguments to identify the data.
         *  
         *  @returns A @ref const_iterator_range containing the data.
         *  
         *  @see @ref Data
         */
        template<typename... Args>
        const_iterator_range RawData(Args &&...args) const;

        /**
         *  Obtain the data.
         *  
         *  This data is usually not raw bytes. The return type is determined by the @p header.
         *  
         *  @tparam Args The types for the arguments to identify the data.
         *  
         *  @param args The arguments to identify the data.
         *  
         *  @returns The data.
         *  
         *  @see @ref RawData
         */
        template<typename... Args>
        auto Data(Args &&...args) const { return Header().Data(std::forward<Args>(args)...); }

        /**
         *  Check whether the resource was initialized.
         *  
         *  @returns @c true if it is loaded, @c false otherwise.
         */
        bool IsLoaded() const { return raw.empty() && (AssertSize(sizeof(*this)), true); }

        /**
         *  Check whether the resource looks sane.
         *  
         *  The @p header may define a @ref resource_base::Check function to check the header.
         *  
         *  @returns @c true if it looks sane, @c false otherwise.
         */
        bool Check() const { return IsLoaded() && Header().Check(); }

    protected:
        /**
         *  Assert the resource having a certain size.
         */
        void AssertSize(std::size_t size) const { ASSERT(raw.size() >= size); }

        //This allows implementation to either return a @p T directly, or specify an offset.
        /**
         *  A helper function for @ref SubHeader.
         *  
         *  @tparam T The sub-header type.
         *  
         *  @param thing The sub-header.
         *  
         *  @note This just copies the data starting from the sub-header to the end of this resource.
         *  
         *  @returns @ref raw_storage to the sub-header.
         */
        //note: these implementations will have to change for memory mapped files
        template<typename T>
        raw_storage Convert(const T& thing) {
            using std::cbegin;
            auto begin = reinterpret_cast<raw_storage::const_iterator>(&thing);
            return Convert(begin - cbegin(raw));
        }

        /**
         *  A helper function for @ref SubHeader.
         *  
         *  @tparam T The sub-header type.
         *  
         *  @param offset The offset to the sub-header.
         *  
         *  @note This just copies the data starting from the sub-header to the end of this resource.
         *  
         *  @returns @ref raw_storage to the sub-header.
         */
        template<typename T>
        raw_storage Convert(std::size_t offset) const;

#if 0 //unsure if needed
        /**
         *  A helper function for @ref SubHeader.
         *  
         *  @tparam T The sub-header type.
         *  
         *  @param filename The filename referring to the file that contains the sub-header.
         *  
         *  @returns @ref raw_storage to the sub-header.
         */
        template<typename T>
        T Convert(const std::string &filename) const
        {
            //this would conflict with Convert(const T&) above
            static_assert(!std::is_same_v<T, std::string>, "T cannot be std::string");
            AssertSize(offset);
            T thing;
            thing.LoadFromFile(filename);
            return std::move(thing);
        }
#endif

    private:
        //Prevent dynamic instantiation since we have no virtual dtor.
        static void* operator new(std::size_t) = delete;
        static void* operator new[](std::size_t) = delete;
        static void operator delete(void*) = delete;
        static void operator delete[](void*) = delete;

    protected:
        raw_storage raw; /**< The raw bytes making up this @ref resource. */
    };

    template<typename header_type>
    template<typename... Args>
    typename resource<header_type>::const_iterator_range resource<header_type>::RawData(Args &&...args) const
    {
        using std::cbegin;
        using std::cend;
        using std::next;
        ASSERT(IsLoaded());
        const auto offset = Header().DataOffset(std::forward<Args>(args)...);
        ASSERT(offset >= 0 && offset <= raw.size());
        ASSERT(offset <= std::numeric_limits<std::make_signed_t<decltype(offset)>>::max());
        const auto dataBegin = next(cbegin(raw), static_cast<std::make_signed_t<decltype(offset)>>(offset));
        const auto left = cend(raw) - dataBegin;
        ASSERT(left >= 0);
        const auto size = Header().DataSize(static_cast<std::make_unsigned_t<decltype(left)>>(left), std::forward<Args>(args)...);
        ASSERT(size >= 0 && size <= std::numeric_limits<std::make_signed_t<decltype(size)>>::max() && offset + size <= raw.size());
        return const_iterator_range{dataBegin, next(dataBegin, static_cast<std::make_signed_t<decltype(size)>>(size))};
    }

    template<typename header_type>
    template<typename T>
    typename resource<header_type>::raw_storage resource<header_type>::Convert(std::size_t offset) const {
        using std::cbegin;
        using std::cend;
        using std::next;
        //this would be in conflict with Convert(const T&) above
        static_assert(!std::is_same<T, std::size_t>::value, "T cannot be std::size_t");
        static_assert(std::is_standard_layout<T>::value, "T must be standard layout");
        AssertSize(offset);
        ASSERT(offset <= std::numeric_limits<std::ptrdiff_t>::max());
        return {next(cbegin(raw), static_cast<std::ptrdiff_t>(offset)), cend(raw)};
    }

} }

#endif // SH3_ARC_RESOURCE_HPP_INCLUDED
