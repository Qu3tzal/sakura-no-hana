#include "Component.hpp"

namespace kantan
{
    /// Ctor.
    Component::Component(std::string name)
		: m_name(name)
    {}

    /// Dtor.
    Component::~Component()
    {}

    /// Id.
	std::string Component::getName() const
	{
		return m_name;
	}
} // namespace kantan.
