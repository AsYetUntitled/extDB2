#include <new>
#include <jemalloc/jemalloc.h>

#include <cstdlib>


void* operator new (size_t size)
{
	if (size == 0) size = 1;
	void* ptr = je_malloc(size);
	if (ptr == NULL)
	{
		throw std::bad_alloc();
	}
	return ptr;
}

void* operator new[] (size_t size)
{
	void* ptr = je_malloc(size);
	if (ptr == NULL)
	{
		throw std::bad_alloc();
	}
	return ptr;
}

void* operator new (size_t size, const std::nothrow_t&)
{
	if (size == 0) size = 1;
	void* ptr = je_malloc(size);
	if (ptr == NULL)
	{
		return ptr;
	}
	else
	{
		return NULL;
	}
}

void* operator new[] (size_t size, const std::nothrow_t&)
{
	void* ptr = je_malloc(size);
	if (ptr == NULL)
	{
		return ptr;
	}
	else
	{
		return NULL;
	}
}

void operator delete (void* ptr)
{
	if (ptr != NULL)
	{
		je_free(ptr);
	}
}

void operator delete[] (void* ptr)
{
	operator delete (ptr);
}

void operator delete (void* ptr, const std::nothrow_t&)
{
	if (ptr != NULL)
	{
		je_free(ptr);
	}
}

void operator delete[] (void* ptr, const std::nothrow_t&)
{
	operator delete(ptr);
}
