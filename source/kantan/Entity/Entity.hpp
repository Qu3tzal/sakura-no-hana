#ifndef KANTAN_ENTITY
#define KANTAN_ENTITY

#include <string>
#include <unordered_map>

namespace kantan
{
    class Component;

	/**
		Entity class.
	**/
	class Entity
	{
		public:
			// Ctor.
			Entity(std::string name);

			// Id.
			unsigned int getId();
			std::string getName();

			// Components.
			void addComponent(Component* comp);
			void removeComponent(std::string name);
			bool hasComponent(std::string name);

			template<typename T>
			T* getComponent(std::string name);

			std::unordered_map<std::string, Component*> getAllComponents();

		protected:
			// Id.
			unsigned int m_id;
			std::string m_name;

			// Components.
			std::unordered_map<std::string, Component*> m_components;

		/// Static :
		protected:
			static unsigned int m_lastid;
	};

	/// Components.
	template<typename T>
	T* Entity::getComponent(std::string name)
	{
		auto it = m_components.find(name);

		if(it != m_components.end())
			return dynamic_cast<T*>(it->second);
		else
			return nullptr;
	}
} // namespace kantan.

#endif // KANTAN_ENTITY
