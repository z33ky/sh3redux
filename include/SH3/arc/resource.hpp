#ifndef SH3_ARC_RESOURCE_HPP_INCLUDED
#define SH3_ARC_RESOURCE_HPP_INCLUDED

#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#include <boost/range/iterator_range_core.hpp>

#include "SH3/arc/mft.hpp"
#include "SH3/system/assert.hpp"

#define SUB_HEADER_IMPL_STRUCT

namespace sh3 { namespace arc {

    struct mft;


#if 0
    //PACKED?
    struct my_header : public resource_base<my_header>
    {
        std::uint16_t magic;
        std::uint64_t dataSize;

        bool Check() const { return magic == 0x76543210; }
        std::size_t DataSize() const { return dataSize; }
    };

    //PACKED?
    struct my_sub_header : public resource_base<my_sub_header>
    {
        std::uint64_t dataOffset; //offset to data after this header in bytes

        constexpr bool Check() const { return true; }
        constexpr std::size_t DataSize() const { return 0x40; } //fixed size data
        std::size_t DataOffset() const { return dataOffset; }
    };
#endif
    template<typename header>
    struct resource;

    template<typename header, typename resource_type = resource<header>>
    struct resource_base
    {
        //static_assert(std::is_standard_layout<header>::value, "The header must be standard layout");
        //static_assert(sizeof(resource_base) == 0, "resource_base cannot contain any member variables, since we alias the resource memory");

    protected:
        constexpr std::size_t DataOffset() const { return sizeof(header); }
        //we might need to revert to template fuckery for this, since this is problematic if header implements DataOffset(size_t), but not DataSize(size_t)
        constexpr std::size_t DataSize(std::size_t len) const { return len; }

        constexpr std::size_t HeaderOffset() const { return 0; }

    protected:

    public:
        //using PrimitiveHeader = header;
        using resource = resource_type;

        //It would be kinda pretty to have the header being the resource,
        //but for one we can't implement IsLoaded() since `this` must not be nullptr. And neither RawData() (at least not with a range) or AssertSize().
        #if 0
        //TODO: Args...
        template<typename T>
        const T& SubHeader(std::size_t idx) const { ASSERT(IsLoaded()); return Convert<T>(header::SubHeaderImpl(idx, static_cast<T*>(nullptr))); }

        #ifdef SUB_HEADER_IMPL_STRUCT
        template<typename T>
        const T& SubHeader() { ASSERT(IsLoaded()); return Convert<T>(header::template get_sub_header<T>::Offset(static_cast<header&>(*this))); }
        #else
        template<typename T>
        const T& SubHeader() const { ASSERT(IsLoaded()); return Convert<T>(header::SubHeaderImpl(static_cast<T*>(nullptr))); }
        #endif

        //FIXME
        //bool IsLoaded() const { if(raw.empty()) return false; AssertSize(sizeof(*this)); return true; }
        bool IsLoaded() const { return this != nullptr; }
        //protected?
        //FIXME
        void AssertSize(std::size_t size) const { ASSERT(sizeof(*this) >= size); }
        #endif

    protected:
        resource_base() = default;
        //TODO
        resource_base(const resource_base&) = default;
        resource_base(resource_base&&) = default;
        ~resource_base() = default;

        resource_base& operator=(const resource_base&) = default;
        resource_base& operator=(resource_base&&) = default;

        //see above
        #if 0
        template<typename T>
        static T& Convert(T& thing) { return thing; }

        template<typename T>
        static const T& Convert(const T& thing) { return thing; }

        //FIXME: non-const? This is in const data, right?
        template<typename T>
        T& Convert(std::size_t offset) { static_assert(!std::is_same<T, std::size_t>::value, "T cannot be std::size_t"); static_assert(std::is_standard_layout<T>::value, "T must be standard layout"); AssertSize(offset); return *reinterpret_cast<T*>(reinterpret_cast<char*>(this) + offset); }

        template<typename T>
        const T& Convert(std::size_t offset) const { static_assert(!std::is_same<T, std::size_t>::value, "T cannot be std::size_t"); static_assert(std::is_standard_layout<T>::value, "T must be standard layout"); AssertSize(offset); return *reinterpret_cast<const T*>(reinterpret_cast<char*>(this) + offset); }

#if 0 //unsure if needed
        template<typename T>
        T Convert(const std::string &filename) const { static_assert(!std::is_same_v<T, std::string>, "T cannot be std::string"); AssertSize(offset); T thing; thing.LoadFromFile(filename); return std::move(thing); }
#endif
        #endif
    };

    template<typename header_type>
    struct resource
    {
    protected:
        //switch to std::uint8_t* const when we change to mmapped files
        using raw_storage = std::vector<std::uint8_t>;

    public:
        using header = header_type;

    public:
        resource() { /*raw.reserve(sizeof(header));*/ /*sizeof(header) + header::SizeHint()*/ }
        resource(raw_storage&& data): raw{std::move(data)} {}
        resource(resource&&) = default;
        ~resource() = default;

        //should be fine though
        //resource& operator=(const resource&) = default;
        resource& operator=(resource&&) = default;

        //FIXME: partial read -> e == true; check return.
        void LoadFromFile(mft& mft, const std::string& filename, typename header::load_result& e) { ASSERT(!IsLoaded()); mft.LoadFile(filename, raw/*, e*/); if(IsLoaded() && !Header().Check()) { raw.clear(); /*FIXME: error*/ } }

        //FIXME: can we have multiple headers?
        //FIXME: header offset
        const header& Header() const { ASSERT(IsLoaded()); return reinterpret_cast<const header&>(raw.front()); }

        template<typename T>
        typename T::resource SubHeader(std::size_t idx) const { ASSERT(IsLoaded()); return Convert<T>(Header().SubHeaderImpl(idx, static_cast<T*>(nullptr))); }

        #ifdef SUB_HEADER_IMPL_STRUCT
        template<typename T>
        typename T::resource SubHeader() const { ASSERT(IsLoaded()); return Convert<T>(header::template get_sub_header<T>::Offset(Header())); }
        #else
        template<typename T>
        typename T::resource SubHeader() const { ASSERT(IsLoaded()); return Convert<T>(Header().SubHeaderImpl(static_cast<T*>(nullptr))); }
        #endif

        using const_iterator_range = boost::iterator_range<raw_storage::const_iterator>;
        //TODO: disable -Wclass-varargs
        template<typename... Args>
        const_iterator_range RawData(Args &&...args) const { using std::cbegin; using std::cend; using std::next; ASSERT(IsLoaded()); const auto dataBegin = next(cbegin(raw), Header().DataOffset(std::forward<Args>(args)...)); return const_iterator_range{dataBegin, next(dataBegin, Header().DataSize(cend(raw) - dataBegin, std::forward<Args>(args)...))}; /*ASSERT(Header().DataSize() <= raw.size())*/}

        template<typename... Args>
        auto Data(Args &&...args) const { return Header().Data(std::forward<Args>(args)...); }

        bool IsLoaded() const { if(raw.empty()) return false; AssertSize(sizeof(*this)); return true; }
        void AssertSize(std::size_t size) const { ASSERT(raw.size() >= size); }

    protected:
        template<typename T>
        static raw_storage Convert(const T& thing) { return thing; }

        template<typename T>
        raw_storage Convert(std::size_t offset) const { using std::begin; using std::end; using std::next; static_assert(!std::is_same<T, std::size_t>::value, "T cannot be std::size_t"); static_assert(std::is_standard_layout<T>::value, "T must be standard layout"); AssertSize(offset); return {next(begin(raw), offset), end(raw)}; }

#if 0 //unsure if needed
        template<typename T>
        T Convert(const std::string &filename) const { static_assert(!std::is_same_v<T, std::string>, "T cannot be std::string"); AssertSize(offset); T thing; thing.LoadFromFile(filename); return std::move(thing); }
#endif

    protected:
        raw_storage raw; /**< The raw bytes making up this @ref resource. */
    };

} }

#endif // SH3_ARC_RESOURCE_HPP_INCLUDED
