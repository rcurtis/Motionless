#pragma once

#pragma once

namespace mt
{
	class NonCopyable
	{
	protected:
		NonCopyable() {}

	public:
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	};
}
