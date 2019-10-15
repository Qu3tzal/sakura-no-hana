#ifndef KANTAN_SYSTEM
#define KANTAN_SYSTEM

#include <SFML/System.hpp>
#include <vector>
#include <queue>

namespace kantan
{
    class Entity;
    class Event;

	/**
		System class.
	**/
	class System
	{
		public:
			// Ctor.
			System();

			virtual void update(sf::Time elapsed, std::vector<Entity*>& entities, std::queue<Event*>& eventQueue) = 0;
	};

} // namespace kantan.

#endif // KANTAN_SYSTEM
