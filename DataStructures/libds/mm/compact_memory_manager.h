#pragma once

#include <libds/mm/memory_manager.h>
#include <libds/mm/memory_omanip.h>
#include <libds/constants.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <utility>

namespace ds::mm {

    template<typename BlockType>
    class CompactMemoryManager : public MemoryManager<BlockType> {
    public:
        CompactMemoryManager();
        CompactMemoryManager(size_t size);
        CompactMemoryManager(const CompactMemoryManager<BlockType>& other);
        ~CompactMemoryManager() override;

        BlockType* allocateMemory() override;
        BlockType* allocateMemoryAt(size_t index);
        void releaseMemory(BlockType* pointer) override;
        void releaseMemoryAt(size_t index);
        void releaseMemory();

        size_t getCapacity() const;

        CompactMemoryManager<BlockType>& assign(const CompactMemoryManager<BlockType>& other);
        void changeCapacity(size_t newCapacity);
        void shrinkMemory();
        void clear();
        bool equals(const CompactMemoryManager<BlockType>& other) const;
        void* calculateAddress(const BlockType& data);
        size_t calculateIndex(const BlockType& data);
        BlockType& getBlockAt(size_t index);
        void swap(size_t index1, size_t index2);

        void print(std::ostream& os);

    private:
        size_t getAllocatedBlocksSize() const;
        size_t getAllocatedCapacitySize() const;

    private:
        BlockType* base_;
        BlockType* end_;
        BlockType* limit_;

        static const size_t INIT_SIZE = 4;
    };

    template<typename BlockType>
    CompactMemoryManager<BlockType>::CompactMemoryManager() :
        CompactMemoryManager(INIT_SIZE)
    {
    }

    template<typename BlockType>
    CompactMemoryManager<BlockType>::CompactMemoryManager(size_t size) :
        base_(static_cast<BlockType*>(std::calloc(size, sizeof(BlockType)))),
        end_(base_),
        limit_(base_ + size)
    {
    }

    template<typename BlockType>
    CompactMemoryManager<BlockType>::CompactMemoryManager(const CompactMemoryManager<BlockType>& other) :
        CompactMemoryManager(other.getAllocatedBlockCount())
    {
        this->assign(other);  // sprava sa ako operator priradenia 
    }

    template<typename BlockType>
    CompactMemoryManager<BlockType>::~CompactMemoryManager()
    {
        this->clear();
        //realloc(this->base_, 0);
        free(this->base_);

        this->base_ = nullptr;
        this->end_ = nullptr;
        this->limit_ = nullptr;
    }

    template<typename BlockType>
    BlockType* CompactMemoryManager<BlockType>::allocateMemory()
    {
        this->allocateMemoryAt(this->end_ - this->base_);
    }

    template<typename BlockType>
    BlockType* CompactMemoryManager<BlockType>::allocateMemoryAt(size_t index)
    {
        if (this->end_ == this->limit_) {
            this->changeCapacity(this->getCapacity() > 0 ? this->getCapacity() * 2 : INIT_SIZE);
        }

        BlockType* src = this->base_ + index;
        BlockType* dest = this->base_ + index + 1;
        size_t size = (this->end_ - index) * sizeof(BlockType);
        std::memmove(dest, src, size);
        
        ++this->end_;
        ++this->allocatedBlockCount_;

        return placement_new(src);  // zavola sa konstruktor tam kde sme alokovali pamat
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::releaseMemory(BlockType* pointer) {
		BlockType* cur = pointer;
		while (cur != this->end_) {
			//destroy(cur);
			cur->~BlockType();
			++cur;
		}

		this->end_ = pointer;
        this->allocatedBlockCount_ = this->end_ - this->base_;
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::releaseMemoryAt(size_t index)
	{
        // TODO
	}

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::releaseMemory()
    {
        this->releaseMemory(this->end_ - 1);
    }

    template<typename BlockType>
    size_t CompactMemoryManager<BlockType>::getCapacity() const
    {
        return this->limit_ - this->base_;
    }

    template<typename BlockType>
    CompactMemoryManager<BlockType>& CompactMemoryManager<BlockType>::assign
    (const CompactMemoryManager<BlockType>& other)
    {
        // memmove ak sa neprekryvaju
        // memcpy ak sa prekryvaju
        // alebo naopak?

        if (this != &other) {
            this->clear();
            this->changeCapacity(other.getCapacity());

            BlockType* thisCur = this->base_;
            BlockType* otherCur = other.base_;

            while (otherCur != other.end_) {
                placement_copy(thisCur, *otherCur);  // vyvolava sa kopirovaci konstruktor other

                ++thisCur;
                ++otherCur;
            }

            this->allocatedBlockCount_ = other.allocatedBlockCount_;
            this->end_ = this->base_ + this->getAllocatedBlockCount();
        }

        return *this;
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::shrinkMemory()
    {
        size_t newCapacity = static_cast<size_t>(end_ - base_);

        if (newCapacity < CompactMemoryManager<BlockType>::INIT_SIZE)
        {
            newCapacity = CompactMemoryManager<BlockType>::INIT_SIZE;
        }

        this->changeCapacity(newCapacity);
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::changeCapacity(size_t newCapacity)
    {
        if (newCapacity != this->getCapacity()) {
            if (newCapacity < this->getAllocatedBlockCount()) {
                this->releaseMemory(this->base_ + newCapacity);
            }
            BlockType* newBase = static_cas<BlockType>*(realloc(this->base_, newCapacity * sizeof(BlockType)));

            if (newBase != this->base_) {
                this->base_ = newBase;
                this->end_ = this->base_ + this->getAllocatedBlockCount();
            }

            this->limit_ = this->base_ + newCapacity;
        }

        //if (newCapacity == this->getCapacity()) {
        //    return;
        //}
        //else if (newCapacity > this->getCapacity()) {
        //    BlockType* newBase = realloc(this->base_, newCapacity * sizeof(BlockType));

        //    if (newBase != this->base_) {
        //        this->base_ = newBase;
        //        this->end_ = this->base_ + this->getAllocatedBlockCount();
        //    }

        //    this->limit_ = this->base_ + newCapacity;
        //}
        //else {
        //    if (newCapacity < this->getAllocatedBlockCount()) {
        //        this->allocatedBlockCount_ = newCapacity;
        //    }

        //    BlockType* newBase = realloc(this->base_, newCapacity * sizeof(BlockType));

        //    if (newBase != this->base_) {
        //        this->base_ = newBase;
        //        this->end_ = this->base_ + this->getAllocatedBlockCount();
        //    }

        //    this->limit_ = this->base_ + newCapacity;
        //}
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::clear()
    {
        this->releaseMemory(this->base_);
    }

    template<typename BlockType>
    bool CompactMemoryManager<BlockType>::equals(const CompactMemoryManager<BlockType>& other) const
    {
        if (this == &other) {
            return true;
        }
        else if (this->getAllocatedBlockCount() != other.getAllocatedBlockCount()) {
            return false;
        }
        else {
            return memcpm(this->base_, other.base_, this->getAllocatedBlocksSize()) == 0;
        }
    }

    template<typename BlockType>
    void* CompactMemoryManager<BlockType>::calculateAddress(const BlockType& data)
    {
        BlockType* p = this->base_;
        while (p != this->end_ && p != &data) {
            p++;
        }
        if (p == this->end_) {
            return nullptr;
        }
        else {
            return p;
        }

        //p == this->end_ ? return nullptr : return p;
    }

    template<typename BlockType>
    size_t CompactMemoryManager<BlockType>::calculateIndex(const BlockType& data)
    {
        if (&data < this->end_ && &data >= this->base_) {
            return &data - this->base_;
        }
        else {
            return INVALID_INDEX;
        }
    }

    template<typename BlockType>
    BlockType& CompactMemoryManager<BlockType>::getBlockAt(size_t index)
    {
        // kontrola indexu sa bude riesit inde
        return *(this->base_ + index);
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::swap(size_t index1, size_t index2)
    {
        std::swap(this->getBlockAt(index1), this->getBlockAt(index2));
    }

    template<typename BlockType>
    size_t CompactMemoryManager<BlockType>::getAllocatedBlocksSize() const
    {
        return (this->end_ * this->base_) * sizeof(BlockType);
    }

    template<typename BlockType>
    size_t CompactMemoryManager<BlockType>::getAllocatedCapacitySize() const
    {
        return (this->limit_ * this->base_) * sizeof(BlockType);
    }

    template<typename BlockType>
    void CompactMemoryManager<BlockType>::print(std::ostream& os)
    {
        os << "first = " << base_ << std::endl;
        os << "last = " << end_ << std::endl;
        os << "limit = " << limit_ << std::endl;
        os << "block size = " << sizeof(BlockType) << "B" << std::endl;

        BlockType* ptr = base_;
        while (ptr != limit_)
        {
            std::cout << ptr;
            os << PtrPrintBin<BlockType>(ptr);

            if (ptr == base_) {
                os << "<- first";
            }
            else if (ptr == end_) {
                os << "<- last";
            }
            os << std::endl;
            ++ptr;
        }

        os << limit_ << "|<- limit" << std::endl;
    }

}