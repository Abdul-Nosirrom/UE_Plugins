// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

/**
 * Implements a lock-free first-in first-out queue using a circular array.
 *
 * This class is thread safe only in single-producer single-consumer scenarios.
 *
 * The number of items that can be enqueued is one less than the queue's capacity,
 * because one item will be used for detecting full and empty states.
 *
 * There is some room for optimization via using fine grained memory fences, but
 * the implications for all of our target platforms need further analysis, so
 * we're using the simpler sequentially consistent model for now.
 *
 * @param T The type of elements held in the queue.
 */
template<typename T>
class COREFRAMEWORK_API TBufferContainer
{
public:
	using FElementType = T;

	TBufferContainer() : Capacity(16), Start(0), End(0), Size(0) {};
	
	explicit TBufferContainer(uint32 BufferCapacity) : 
		Capacity(BufferCapacity)
		, Start(0), End(0), Size(0)
	{
		Buffer.AddZeroed(BufferCapacity);
	}

	/* Setter */
	FORCEINLINE FElementType& operator[](uint32 Index)
	{
		return Buffer[InternalIndex(Index)];
	}

	/* Getter */
	FORCEINLINE FElementType operator[](uint32 Index) const
	{
		return Buffer[InternalIndex(Index)];
	}
	
	int InternalIndex(uint32 Index) const
	{
		return Start + (Index < (Capacity - Start) ? Index : Index - Capacity);
	}

	void PushFront(FElementType Item)
	{
		if (IsFull())
		{
			Decrement(Start);
			End = Start;
			Buffer[End] = Item;
		}
		else 
		{
			Decrement(Start);
			Buffer[Start] = Item;
			++Size;
		}
	}

	void PushBack(FElementType Item)
	{
		if (IsFull())
		{
			Buffer[End] = Item;
			Increment(End);
			Start = End;
		}
		else 
		{
			Buffer[End] = Item;
			Increment(End);
			++Size;
		}
	}

	void PopFront()
	{
		//Buffer[End] = null; // Needs to be just a default of FElementType
		Increment(Start);
		--Size;
	}

	void PopBack()
	{
		Decrement(End);
		//Buffer[End] = null; // Needs to be just a default of FElementType
		--Size;
	}

	void Clear()
	{
		Size = 0;
		Start = 0;
		End = 0;
		Buffer.Clear();
	}


	FORCEINLINE int GetCapacity() const { return Capacity; }

	FORCEINLINE int GetSize() const { return Size; }

	FORCEINLINE bool IsEmpty() const { return Size == 0; }

	FORCEINLINE bool IsFull() const { return Size == Capacity; }

	FElementType Front() const { return Buffer[Start]; }

	FElementType Back() const { return Buffer[(End != 0 ? End : Capacity) - 1]; }

protected:

	void Increment(uint32& Index)
	{
		if (++Index == Capacity)
		{
			Index = 0;
		}
	}

	void Decrement(uint32& Index)
	{
		if (Index == 0)
		{
			Index = Capacity;
		}
		Index--;
	}
	/* Holds the buffer */
	TArray<FElementType> Buffer;

	uint32 Capacity;
	uint32 Start;
	uint32 End;
	uint32 Size;
};
