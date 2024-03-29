#ifndef KANTAN_RESOURCE_HOLDER
#define KANTAN_RESOURCE_HOLDER

#include <SFML/Graphics.hpp>

#include <map>
#include <memory>
#include <string>
#include <exception>
#include <cassert>

namespace kantan
{
    /*
        ResourceHolder class.
    */
    template<typename Resource, typename Identifier>
    class ResourceHolder
    {
        public:
            // Loads a resource from a file.
            void load(Identifier id, const std::string& filename);

            // Overloaded load method for sf::Shaders.
            template<typename Parameter>
            void load(Identifier id, const std::string& filename, const Parameter& secondParameter);

            // Returns a reference to the resource.
            Resource& get(Identifier id);
            const Resource& get(Identifier id) const;

            // Unload a resource.
            void unload(Identifier id);

        protected:
            // Insert a resource into the resources map.
            void insertResource(Identifier id, std::unique_ptr<Resource> resource);

            // Textures holder.
            std::map<Identifier, std::unique_ptr<Resource>> m_resourceMap;
    };

    // Include template definition.
    #include "ResourceHolder.inl"

    /*
        Useful typedef.
    */
    typedef ResourceHolder<sf::Texture, unsigned int> TextureHolder;
    typedef ResourceHolder<sf::Font, unsigned int> FontHolder;
} // namespace kantan.

#endif // KANTAN_RESOURCE_HOLDER
