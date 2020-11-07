#pragma once

#include "id_pool.h"

template<typename T, typename Id = size_t>
class SparseArray
{
public:
	class iterator
    {
        public:
            typedef iterator self_type;
            typedef T value_type;
            typedef T& reference;
            typedef T* pointer;
            typedef std::forward_iterator_tag iterator_category;
            typedef int difference_type;
            iterator(std::optional<T>* ptr) : ptr(ptr) { }
            self_type operator++(){ self_type i = *this; advance(); return i; }
            reference operator*() { return ptr->object; }
            pointer operator->() { return &ptr->object; }
            bool operator==(const self_type& rhs) { return ptr == rhs.ptr; }
            bool operator!=(const self_type& rhs) { return ptr != rhs.ptr; }

            void advance()
            {
            	while(ptr++);
            }
        private:
            std::optional<T>* ptr;
    };

	class const_iterator
    {
        public:
            typedef const_iterator self_type;
            typedef T value_type;
            typedef T& reference;
            typedef T* pointer;
            typedef const T& reference_const;
            typedef const T* pointer_const;
            typedef std::forward_iterator_tag iterator_category;
            typedef int difference_type;
            const_iterator(std::optional<T>* ptr) : ptr(ptr) { }
            self_type operator++(){ self_type i = *this; advance(); return i; }
            reference_const operator*() { return ptr->object; }
            pointer_const operator->() { return &ptr->object; }
            bool operator==(const self_type& rhs) { return ptr == rhs.ptr; }
            bool operator!=(const self_type& rhs) { return ptr != rhs.ptr; }

            void advance()
            {
            	while(ptr++);
            }
        private:
            std::optional<T>* ptr;
    };

    iterator begin()
    {
        return iterator(buffer.data());
    }

    iterator end()
    {
        return iterator(buffer.data() + buffer.size() - 1);
    }

    const_iterator begin() const
    {
        return const_iterator(buffer.data());
    }

    const_iterator end() const
    {
        return const_iterator(buffer.data() + buffer.size() - 1);
    }

	std::pair<Id, T&> next()
	{
		const Id id = idPool.next();
		if(id >= (buffer.size() - 1))
		{
            buffer.resize(id + 2);
		}
        buffer[id] = T();
		return {id, buffer[id].value()};
	}

	void free(Id id)
	{
		idPool.free(id);
	}

	SparseArray()
	{
		buffer.resize(1);
	}

	T& operator[](Id id)
	{
		return buffer[id].value();
	}

	const T& operator[](Id id) const
	{
		return buffer[id].value();
	}

	std::vector<std::optional<T>> buffer;
	IdPool<Id, true> idPool;
};