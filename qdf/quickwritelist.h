#pragma once

#include <stdlib.h>
#include <string.h>

// Is this probably overkill? Most likely...

namespace ng::qdf {

	// Used for bulk storing and allocating elements in blocks of memory
	template <typename T>
	class QuickWriteList
	{
		class Block;
	public:
		// Used for skipping to locations in a list 
		class Location
		{
		public:
			Block* walkingBlock;
			size_t walkingPos;

			T* next();
			T* prev();
			T* cur();

			Location& operator++() { next(); return *this; }
			Location operator++(int) { Location tmp{ walkingBlock, walkingPos }; operator++(); return tmp; }
			bool operator==(const Location& rhs) const { return walkingPos == rhs.walkingPos && walkingBlock == rhs.walkingBlock; }
			bool operator!=(const Location& rhs) const { return walkingPos != rhs.walkingPos || walkingBlock != rhs.walkingBlock; }
			T& operator*() { return *cur(); }
		};

		// Block size is the initial size of a block
		// Block increment size is how much the block grows on each new allocation
		// Block size cap is the max size of a block
		QuickWriteList(size_t blockSize = 8, size_t blockIncrementSize = 4, size_t blockMultiplySize = 1, size_t blockSizeCap = 128);
		~QuickWriteList();

		// Finds or allocates a new element
		T* add();
		inline T* add(T val) { T* p = add(); *p = val; return p; }

		void clear();

		// PERMANENTLY merges another list into the current list via linking the blocks, and CLEARS it.
		void fastMerge(QuickWriteList<T>& list);

		// Merges another list into the current list one element at a time
		void cleanMerge(QuickWriteList<T>& list);

		// PERMANENTLY merges another list into the current list one element at a time until it can link blocks
		// CLEARS THE OTHER LIST
		void mixMerge(QuickWriteList<T>& list);

		// Copies out all data into an array. Array is allocated with malloc!
		T* compact();

		size_t count() { return elementCount; };

		Location begin() { Location l{ firstBlock, 0 }; if (firstBlock->next && firstBlock->next->count > 0) l.walkingBlock = firstBlock->next; return l; }
		Location end() { return { currentBlock, currentBlock->count }; }
		
		T* prev();
		T* next();

		T* cur() { return loc.cur(); }

		void resetHead() { loc.walkingBlock = firstBlock->next ? firstBlock->next : firstBlock; loc.walkingPos = 0; }

		Location endLocation() { return { currentBlock, currentBlock->count-1 }; }
		void setLocation(Location l) { loc = l; }
		Location location() { return loc; }
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
		size_t blockMultiplySize;
		size_t blockSizeCap;

		// Used for next and prev
		Location loc;
	};


	// I really wish C++ could just let me use a cpp file...

	template<typename T>
	inline QuickWriteList<T>::QuickWriteList(size_t blockSize, size_t blockIncrementSize, size_t blockMultiplySize, size_t blockSizeCap)
	{
		this->blockSize = blockSize;
		this->blockIncrementSize = blockIncrementSize;
		this->blockMultiplySize = blockMultiplySize;
		this->blockSizeCap = blockSizeCap;

		elementCount = 0;

		// First block has a size of 0 for ease of logic... Maybe fix that?
		loc.walkingBlock = currentBlock = firstBlock = new Block(0, nullptr);
		loc.walkingPos = 0;

		curBlockSize = blockSize;
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
		curBlockSize = blockSize;
	}

	template<typename T>
	inline void QuickWriteList<T>::fastMerge(QuickWriteList<T>& list)
	{
		// To merge in another list, we just take it's first block and tack it onto the end of our linked list
		// It wastes memory, but it's quick!
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
	inline void QuickWriteList<T>::cleanMerge(QuickWriteList<T>& list)
	{
		if (list.elementCount > 0 && list.firstBlock->next)
		{
			// Add in all of another list's elements one by one
			// Slower, but nicer on both of the lists
			list.resetHead();
			for (T* e = list.begin(); e; e = list.next())
			{
				*add() = *e;
			}
			list.resetHead();
		}
	}

	template<typename T>
	inline void QuickWriteList<T>::mixMerge(QuickWriteList<T>& list)
	{
		if (list.elementCount > 0 && list.firstBlock->next)
		{
			// Add one by one until we hit our cap
			list.resetHead();
			for (T* e = list.begin(); e; e = list.next())
			{
				if (currentBlock->count < currentBlock->capacity)
				{
					*add() = *e;
					list.elementCount--;
				}
				else
				{
					break;
				}
			}

			if (list.loc.walkingBlock && list.loc.walkingPos < list.loc.walkingBlock->count)
			{
				if (list.loc.walkingPos == 0 && (list.loc.walkingBlock->count == list.loc.walkingBlock->capacity || !list.loc.walkingBlock->next))
				{
					// This is full. Just link it
					if (list.loc.walkingBlock->prev)
					{
						// This trips up the for loop in clear and causes it to not delete anything
						list.loc.walkingBlock->prev->next = nullptr;
					}
					currentBlock->next = list.loc.walkingBlock;
					list.loc.walkingBlock->prev = currentBlock;
					currentBlock = list.currentBlock;
					elementCount += list.elementCount;
				}
				else
				{
					// Allocate a block for our remaining elements on our walked list
					Block* b = new Block(list.loc.walkingBlock->count - list.loc.walkingPos, currentBlock);
					memcpy(b->elements, list.loc.walkingBlock->elements + list.loc.walkingPos, b->capacity * sizeof(T));
					b->count = b->capacity;
					currentBlock->next = b;
					currentBlock = b;
					elementCount += b->count;
					list.elementCount -= b->capacity;

					if (list.loc.walkingBlock->next)
					{
						list.loc.walkingBlock->next->prev = currentBlock;

						currentBlock->next = list.loc.walkingBlock->next;
						currentBlock = list.currentBlock;
						elementCount += list.elementCount;

						// This trips up the for loop in clear and causes it to not delete anything
						list.loc.walkingBlock->next = nullptr;
					}
				}
			}

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
		return loc.prev();
	}

	template<typename T>
	inline T* QuickWriteList<T>::next()
	{
		// Check if we havent changed loc.walkingBlock to be the actual start
		if (loc.walkingBlock == firstBlock)
			if (firstBlock->next)
				loc.walkingBlock = firstBlock->next;
			else
				return nullptr;

		return loc.next();
	}

	template<typename T>
	inline void QuickWriteList<T>::allocNextBlock()
	{
		if(currentBlock != firstBlock)
		{
			if (curBlockSize < blockSizeCap)
				curBlockSize = (curBlockSize * blockMultiplySize + blockIncrementSize);
			else
				curBlockSize = blockSizeCap;
		}

		currentBlock = currentBlock->next = new Block(curBlockSize, currentBlock);
	}

	template<typename T>
	inline T* QuickWriteList<T>::Location::next()
	{
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
	inline T* QuickWriteList<T>::Location::prev()
	{
		walkingPos--;
		if (walkingPos < 0)
		{
			if (walkingBlock->prev)
			{
				walkingBlock = walkingBlock->prev;
				walkingPos = walkingBlock->count - 1;
			}
			else
				return nullptr;
		}
		return &walkingBlock->elements[walkingPos];
	}

	template<typename T>
	inline T* QuickWriteList<T>::Location::cur()
	{
		if(walkingPos < walkingBlock->count)
			return &walkingBlock->elements[walkingPos];
		return nullptr;
	}

};