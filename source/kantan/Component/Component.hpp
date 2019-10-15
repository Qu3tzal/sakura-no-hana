#ifndef KANTAN_COMPONENT
#define KANTAN_COMPONENT

#include <string>

namespace kantan
{
	/**
		Component class.
	**/
	class Component
	{
		public:
			// Ctor.
			Component(std::string name = "Unknown");

			// Dtor.
			virtual ~Component();

			// Id.
			virtual std::string getName() const;

		protected:
			std::string m_name;
	};

} // namespace kantan.

#endif // KANTAN_COMPONENT
