/** @file
 *  Defines the @ref sh3::system::resource_manager.
 *  
 *  @copyright 2017  Palm Studios
 */
#ifndef SH3_SYSTEM_RESOURCE_MANAGER_HPP_INCLUDED
#define SH3_SYSTEM_RESOURCE_MANAGER_HPP_INCLUDED

#include <iterator>
#include <map>
#include <utility>

/**
 *  Resources have two representations:
 *    * serialized ("on disk", i.e. a file)
 *    * deserialized ("in memory", i.e. an OpenGL texture)
 *  
 *  Whenever a resource needs to be loaded, the @ref resource_manager looks if it is already loaded
 *  and returns a @ref resource_manager::resource_ref.
 *  If not, it is loaded synchronously.
 *  
 *  The @ref resource_manager::resource_ref provides automatic management of the resource via an embedded reference count.
 *  When all references disappear, the resource will be unloaded.
 *  
 *  For the future it'll be better to just unload all resources on fixed points, e.g. level unload.
 *  For stuff we want to keep loaded (e.g. menu images, fonts, ect.) we can just use boolean flags.
 *  
 *  The current implementation cannot be used multi-threaded.
 *  To make it multi-threaded, @ref resource_manager::loadedResources needs to support atomic insertion and removal and resource loading must happen asynchronously.
 *  Only changing the reference count of @ref resource_manager::resource_ref to be atomic would be sufficient for multi-threaded resource sharing provided they are loaded and unloaded from a single-threaded context.
 */

namespace sh3 { namespace system {
    template<typename resource>
    struct resource_manager final
    {
    private:
        struct resource_block
        {
            resource rc;
            std::size_t referenceCount = 0;
        };
        using resource_map = std::map<std::string, resource_block>;

    public:
        struct resource_ref final
        {
        public:
            resource_ref(resource_manager &manager, typename resource_map::iterator it): rcIt{it}, man{manager} {}
            resource_ref(const resource_ref &other);
            resource_ref(resource_ref&&) = default;
            ~resource_ref();

            resource& operator*() { return rcIt->rc; }
            const resource& operator*() const { return rcIt->rc; }

            resource& operator->() { return rcIt->rc; }
            const resource& operator->() const { return rcIt->rc; }

        private:
            typename resource_map::iterator &rcIt;
            resource_manager &man;
        };

    public:
        resource_ref LoadResource(const std::string &path);
        resource_ref UnloadResource(typename resource_map::iterator);

    private:
        resource_map loadedResources;
    };

    template<typename resource>
    typename resource_manager<resource>::resource_ref resource_manager<resource>::LoadResource(const std::string &path)
    {
        using std::cend;
        auto emplaced = loadedResources.emplace(path, {});
        assert(emplaced.first != cend(loadedResources));
        auto &block = emplaced.first->second;
        if(emplaced.second)
        {
            block.rc.LoadFromFile(path);
            assert(block.referenceCount == 1);
        }
        else
        {
            assert(block.referenceCount > 1);
        }
        return resource_ref{*this, emplaced.first};
    }

    template<typename resource>
    resource_manager<resource>::resource_ref::resource_ref(const resource_ref &other)
        : rcIt{other.rcIt}
        , man{other.man}
    {
        assert(rcIt->referenceCount != 0);
        ++rcIt->referenceCount;
    }

    template<typename resource>
    resource_manager<resource>::resource_ref::~resource_ref()
    {
        assert(rcIt->referenceCount != 0);
        if(--rcIt->referenceCount == 0)
        {
            man.UnloadResource(rcIt);
        }
    }
} }

#endif //SH3_SYSTEM_RESOURCE_MANAGER_HPP_INCLUDED
