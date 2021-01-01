#pragma once
#include <cstdint>

namespace ng::qdf{

	// This class is used for QDF's children and values
	class QDFParser;

	template<typename T>
	class IterArray
	{
		class Iterator;
	public:

		IterArray() { elements = nullptr; elementCount = 0; }
		IterArray(T* data, size_t count) { elements = data; elementCount = count; }

		inline size_t count() { return elementCount; }
		inline T* data() { return elements; }

		Iterator begin() { return { elements }; }
		Iterator end() { return { elements + elementCount }; }

		T& operator[](size_t i) { return elements[i]; }

	private:
		class Iterator
		{
			friend IterArray;
			Iterator(T* _p) { p = _p; }
			T* p;
		public:
			Iterator& operator++() { p++; return *this; }
			Iterator operator++(int) { Iterator tmp{ p }; operator++(); return tmp; }
			bool operator==(const Iterator& rhs) const { return p == rhs.p; }
			bool operator!=(const Iterator& rhs) const { return p != rhs.p; }
			T& operator*() { return *p; }
		};

		T* elements;
		size_t elementCount;

		friend QDFParser;
	};


};