#pragma once

#include <stdlib.h>

// Is this probably overkill? Most likely...

namespace ng::qdf {

	// Used for bulk storing and allocating elements in blocks of memory
	template <typename T>
	class QuickWriteList
	{
	public:
		// Block size is the initial size of a block
		// Block increment size is how much the block grows on each new allocation
		// Block size cap is the max size of a block
		QuickWriteList(size_t blockSize = 8, size_t blockIncrementSize = 2, size_t blockSizeCap = 128);
		~QuickWriteList();

		// Finds or allocates a new element
		T* add();
		inline T* add(T val) { T* p = add(); *p = val; return p; }

		void clear();

		// PERMANENTLY merges another list into the current list, and CLEARS it.
		void merge(QuickWriteList<T>& list);

		// Copies out all data into an array. Array is allocated with malloc!
		T* compact();

		size_t count() { return elementCount; };

		T* begin() { if (firstBlock->next && firstBlock->next->count > 0) return firstBlock->next->elements; return nullptr; }
		T* end() { if (currentBlock->count > 0) return currentBlock->elements + currentBlock->count - 1; if (currentBlock->prev) return currentBlock->prev->elements + currentBlock->prev->count - 1; return nullptr; }
		
		T* prev();
		T* next();


	private:

		void allocNextBlock();

		struct Block
		{
			Block(size_t len, Block* prev)
			{
				elements = new T[len];
				count = 0;
				capacity = len;
				next = nullptr;
				this->prev = prev;
			}
			~Block()
			{
				delete[] elements;
			}

			T* elements;
			size_t count;
			size_t capacity;
			Block* next;
			Block* prev;
		};

		Block* firstBlock;
		Block* currentBlock;

		size_t elementCount;

		size_t curBlockSize;
		size_t blockSize;
		size_t blockIncrementSize;
		size_t blockSizeCap;

		// Used for next and prev
		Block* walkingBlock;
		size_t walkingPos;
	};


	// I really wish C++ could just let me use a cpp file...

	template<typename T>
	inline QuickWriteList<T>::QuickWriteList(size_t blockSize, size_t blockIncrementSize, size_t blockSizeCap)
	{
		this->blockSize = blockSize;
		this->blockIncrementSize = blockIncrementSize;
		this->blockSizeCap = blockSizeCap;

		elementCount = 0;

		// First block has a size of 0 for ease of logic... Maybe fix that?
		walkingBlock = currentBlock = firstBlock = new Block(0, nullptr);
		walkingPos = 0;

		// Due to the first block being 0, curBlockSize needs to be blockSize - blockIncrementSize...
		curBlockSize = blockSize - blockIncrementSize;
	}

	template<typename T>
	inline QuickWriteList<T>::~QuickWriteList()
	{
		for (Block* b = firstBlock; b; )
		{
			Block* nextB = b->next;
			delete b;
			b = nextB;
		}
	}


	template<typename T>
	inline T* QuickWriteList<T>::add()
	{
	get:
		if (currentBlock->count < currentBlock->capacity)
		{
			elementCount++;
			return &currentBlock->elements[currentBlock->count++];
		}
		allocNextBlock();
		goto get;
	}

	template<typename T>
	inline void QuickWriteList<T>::clear()
	{
		// No need to delete the first block, as it's empty
		for (Block* b = firstBlock->next; b; )
		{
			Block* nextB = b->next;
			delete b;
			b = nextB;
		}

		firstBlock->next = nullptr;

		elementCount = 0;
		curBlockSize = blockSize - blockIncrementSize;
	}

	template<typename T>
	inline void QuickWriteList<T>::merge(QuickWriteList<T>& list)
	{
		// To merge in another list, we just take it's first block and tack it onto the end of our linked list
		// It wastes a bit of memory, but it's quick!
		if (list.elementCount > 0 && list.firstBlock->next)
		{
			list.firstBlock->next->prev = currentBlock;
			// first->next since the first block is always blank
			currentBlock->next = list.firstBlock->next;
			currentBlock = list.currentBlock;
			elementCount += list.elementCount;

			// This trips up the for loop in clear, and causes it to not delete anything
			list.firstBlock->next = nullptr;
			list.clear();
		}
	}

	template<typename T>
	inline T* QuickWriteList<T>::compact()
	{
		T* data = (T*)malloc(sizeof(T) * elementCount);
		size_t pos = 0;
		for (Block* b = firstBlock; b; b = b->next)
		{
			for (size_t i = 0; i < b->count; i++)
				data[pos++] = b->elements[i];
		}
		return data;
	}

	template<typename T>
	inline T* QuickWriteList<T>::prev()
	{
		walkingPos--;
		if (walkingPos < 0)
		{
			if (walkingBlock->next)
			{
				walkingPos = 0;
				walkingBlock = walkingBlock->next;
			}
			else
				return nullptr;
		}
		return &walkingBlock->elements[walkingPos];
	}

	template<typename T>
	inline T* QuickWriteList<T>::next()
	{
		// Check if we havent changed walkingBlock to be the actual start
		if (walkingBlock == firstBlock)
			if (firstBlock->next)
				walkingBlock = firstBlock->next;
			else
				return nullptr;

		walkingPos++;
		while (walkingPos >= walkingBlock->count)
		{
			if (walkingBlock->next)
			{
				walkingPos = 0;
				walkingBlock = walkingBlock->next;
			}
			else
				return nullptr;
		}
		return &walkingBlock->elements[walkingPos];
	}

	template<typename T>
	inline void QuickWriteList<T>::allocNextBlock()
	{
		if (curBlockSize < blockSizeCap)
			curBlockSize += blockIncrementSize;
		else
			curBlockSize = blockSizeCap;

		currentBlock = currentBlock->next = new Block(curBlockSize, currentBlock);
	}

};