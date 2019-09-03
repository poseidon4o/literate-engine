#pragma once

#include <stdlib.h>
#include <cassert>

#include <memory>
#include <thread>

#include <vector>
#include <string>
#include <chrono>
#include <iostream>

#define TEMP_MALLOC malloc
#define TEMP_FREE free
#define TEMP_NEW_INPLACE(MEMORY) new(MEMORY)

class LinearAllocator
{
public:
	LinearAllocator(size_t size)
		: m_Ptr{ static_cast<char*>(TEMP_MALLOC(size)) }
		, m_TotalSize{ size }
		, m_FreeSpace{ size }
	{}

	~LinearAllocator()
	{
		TEMP_FREE(m_Ptr);
	}

	void* Allocate(size_t size, unsigned alignment /* power of 2 */)
	{
		//assert((alignment & (alignment - 1)) == 0);

		auto currentPtr = static_cast<void*>(m_Ptr + (m_TotalSize - m_FreeSpace));
		auto retPtr = currentPtr;//std::align(alignment, size, currentPtr, m_FreeSpace);

		if (!retPtr)
		{
			////assert(false && "Linear allocator full!");
			// no space
			return nullptr;
		}

		m_FreeSpace -= size;

		return retPtr;
	}

	void Free()
	{
		// do nothing
	}

	void Reset(size_t freeSpace)
	{
		m_FreeSpace = m_TotalSize;
	}

	size_t CurrentFreeSpace() const { return m_FreeSpace; }

private:
	char* m_Ptr;
	size_t m_TotalSize;
	size_t m_FreeSpace;
};

extern LinearAllocator* tlsLinearAllocator;

template<typename T>
class TempStdAllocator {
public:
	typedef T value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;

public:
	template<typename U>
	struct rebind {
		typedef TempStdAllocator<U> other;
	};

public:
	inline TempStdAllocator() {}
	inline ~TempStdAllocator() {}
	inline TempStdAllocator(const TempStdAllocator& rhs) {}

	template<typename U>
	inline TempStdAllocator(const TempStdAllocator<U>& rhs) {}

	inline pointer address(reference r) { return &r; }
	inline const_pointer address(const_reference r) { return &r; }

	inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0)
	{
		return reinterpret_cast<pointer>(tlsLinearAllocator->Allocate(size_t(cnt * sizeof(T)), sizeof(size_t) == 4 ? 8 : 16));
	}
	inline void deallocate(pointer p, size_type)
	{
		// do nothing
	}

	inline size_type max_size() const
	{
		return std::numeric_limits<size_type>::max() / sizeof(T);
	}

	template <class U, class... Args>
	inline void construct(U* p, Args&&... args)
	{
		TEMP_NEW_INPLACE(p) U(std::forward<Args>(args)...);
	}

	inline void destroy(pointer p) { p->~T(); }

	inline bool operator==(TempStdAllocator const&) const { return true; }
	inline bool operator!=(TempStdAllocator const& a) const { return !operator==(a); }

private:
	template<typename U> friend class TempStdAllocator;
};

#define TEST_TEMP_ALLOC 1
#if TEST_TEMP_ALLOC
using TmpString = std::basic_string<char, std::char_traits<char>, TempStdAllocator<char>>;
template<class T>
using TmpVector = std::vector<T, TempStdAllocator<T>>;
#else
using TmpString = std::basic_string<char, std::char_traits<char>>;
template<class T>
using TmpVector = std::vector<T>;
#endif

struct TempAllocatorScope
{
public:
	TempAllocatorScope()
		: m_Space(tlsLinearAllocator->CurrentFreeSpace())
	{}

	~TempAllocatorScope()
	{
		tlsLinearAllocator->Reset(m_Space);
	}
private:
	size_t m_Space;
};

