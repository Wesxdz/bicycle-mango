#pragma once

template<typename T = size_t, bool Reuse = true>
class IdPool
{
public:
	T next()
	{
		if constexpr(Reuse)
		{
			if(freeIds.size() > 0)
			{
				T id = freeIds.back();
				freeIds.pop_back();
				return id;
			}
		}

		return nextId++;
	}

	void free(T id)
	{
		if constexpr(Reuse)
		{
			freeIds.push_back(id);
		}
	}

public:
	T nextId = 0;
	std::vector<T> freeIds;	
};
