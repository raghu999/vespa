// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vespa/searchcommon/attribute/iattributevector.h>
#include <vespa/searchlib/common/address_space.h>
#include <vespa/searchlib/datastore/datastore.h>
#include <vespa/searchlib/util/memoryusage.h>
#include <vespa/vespalib/util/array.h>
#include <set>
#include <vespa/searchlib/btree/btree.h>
#include <atomic>

namespace vespalib { class asciistream; }
namespace search {

class BufferWriter;

namespace attribute { class Status; }

class EnumStoreBase;
class EnumStoreComparator;
class EnumStoreComparatorWrapper;

typedef datastore::DataStoreT<datastore::AlignedEntryRefT<31, 4> > EnumStoreDataStoreType;
typedef EnumStoreDataStoreType::RefType EnumStoreIndex;
typedef vespalib::Array<EnumStoreIndex> EnumStoreIndexVector;
typedef vespalib::Array<uint32_t> EnumStoreEnumVector;

typedef btree::BTreeTraits<16, 16, 10, true> EnumTreeTraits;

typedef btree::BTree<EnumStoreIndex, btree::BTreeNoLeafData,
                     btree::NoAggregated,
                     const EnumStoreComparatorWrapper,
                     EnumTreeTraits> EnumTree;
typedef btree::BTree<EnumStoreIndex, datastore::EntryRef,
                     btree::NoAggregated,
                     const EnumStoreComparatorWrapper,
                     EnumTreeTraits> EnumPostingTree;

struct CompareEnumIndex
{
    typedef EnumStoreIndex Index;

    inline bool
    operator()(const Index &lhs, const Index &rhs) const
    {
        return lhs.ref() < rhs.ref();
    }
};

class EnumStoreDictBase
{
public:
    typedef EnumStoreIndex Index;
    typedef EnumStoreIndexVector IndexVector;
    typedef EnumStoreEnumVector EnumVector;
    typedef std::set<Index, CompareEnumIndex> IndexSet;
    typedef vespalib::GenerationHandler::generation_t  generation_t;

protected:
    EnumStoreBase &_enumStore;

public:
    EnumStoreDictBase(EnumStoreBase &enumStore);

    virtual
    ~EnumStoreDictBase(void);

    virtual void
    freezeTree(void) = 0;

    virtual uint32_t
    getNumUniques(void) const = 0;
    
    virtual MemoryUsage
    getTreeMemoryUsage(void) const = 0;

    virtual void
    reEnumerate(void) = 0;

    virtual void
    writeAllValues(BufferWriter &writer,
                   btree::BTreeNode::Ref rootRef) const = 0;

    virtual ssize_t
    deserialize(const void *src, size_t available,
                IndexVector &idx) = 0;

    virtual void
    fixupRefCounts(const EnumVector &hist) = 0;

    virtual void
    freeUnusedEnums(const EnumStoreComparator &cmp,
                    const EnumStoreComparator *fcmp) = 0;

    virtual void
    freeUnusedEnums(const IndexVector &toRemove,
                    const EnumStoreComparator &cmp,
                    const EnumStoreComparator *fcmp) = 0;

    virtual bool
    findIndex(const EnumStoreComparator &cmp, Index &idx) const = 0;

    virtual bool
    findFrozenIndex(const EnumStoreComparator &cmp, Index &idx) const = 0;

    virtual void
    onReset(void) = 0;

    virtual void
    onTransferHoldLists(generation_t generation) = 0;

    virtual void
    onTrimHoldLists(generation_t firstUsed) = 0;

    virtual btree::BTreeNode::Ref
    getFrozenRootRef(void) const = 0;

    virtual uint32_t
    lookupFrozenTerm(btree::BTreeNode::Ref frozenRootRef,
                     const EnumStoreComparator &comp) const = 0;

    virtual uint32_t
    lookupFrozenRange(btree::BTreeNode::Ref frozenRootRef,
                      const EnumStoreComparator &low,
                      const EnumStoreComparator &high) const = 0;

    virtual EnumPostingTree &
    getPostingDictionary(void) = 0;

    virtual const EnumPostingTree &
    getPostingDictionary(void) const = 0;

    virtual bool
    hasData(void) const = 0;
};


template <typename Dictionary>
class EnumStoreDict : public EnumStoreDictBase
{
protected:
    Dictionary _dict;

public:
    EnumStoreDict(EnumStoreBase &enumStore);

    virtual
    ~EnumStoreDict(void);

    const Dictionary &
    getDictionary() const
    {
        return _dict;
    }

    Dictionary &
    getDictionary()
    {
        return _dict;
    }
    
    virtual void
    freezeTree(void);

    virtual uint32_t
    getNumUniques(void) const;
    
    virtual MemoryUsage
    getTreeMemoryUsage(void) const;

    virtual void
    reEnumerate(void);

    virtual void
    writeAllValues(BufferWriter &writer,
                   btree::BTreeNode::Ref rootRef) const override;

    virtual ssize_t
    deserialize(const void *src, size_t available,
                IndexVector &idx);

    virtual void
    fixupRefCounts(const EnumVector &hist);

    void
    removeUnusedEnums(const IndexSet &unused,
                      const EnumStoreComparator &cmp,
                      const EnumStoreComparator *fcmp);

    virtual void
    freeUnusedEnums(const EnumStoreComparator &cmp,
                    const EnumStoreComparator *fcmp);

    virtual void
    freeUnusedEnums(const IndexVector &toRemove,
                    const EnumStoreComparator &cmp,
                    const EnumStoreComparator *fcmp);

    virtual bool
    findIndex(const EnumStoreComparator &cmp, Index &idx) const;

    virtual bool
    findFrozenIndex(const EnumStoreComparator &cmp, Index &idx) const;

    virtual void
    onReset(void);

    virtual void
    onTransferHoldLists(generation_t generation);

    virtual void
    onTrimHoldLists(generation_t firstUsed);

    virtual btree::BTreeNode::Ref
    getFrozenRootRef(void) const;

    virtual uint32_t
    lookupFrozenTerm(btree::BTreeNode::Ref frozenRootRef,
                     const EnumStoreComparator &comp) const;

    virtual uint32_t
    lookupFrozenRange(btree::BTreeNode::Ref frozenRootRef,
                      const EnumStoreComparator &low,
                      const EnumStoreComparator &high) const;

    virtual EnumPostingTree &
    getPostingDictionary(void);

    virtual const EnumPostingTree &
    getPostingDictionary(void) const;

    virtual bool
    hasData(void) const;
};


class EnumStoreBase
{
public:
    typedef vespalib::GenerationHandler::generation_t  generation_t;
    typedef attribute::IAttributeVector::EnumHandle     EnumHandle;
    typedef EnumStoreDataStoreType DataStoreType;
    typedef EnumStoreIndex         Index;
    typedef EnumStoreIndexVector   IndexVector;
    typedef EnumStoreEnumVector    EnumVector;

    class EntryBase {
    protected:
        char * _data;
    public:
        EntryBase(void * data)
            :
            _data(static_cast<char *>(data))
        {
        }

        uint32_t
        getEnum() const
        {
            uint32_t *src = reinterpret_cast<uint32_t *>(_data);
            return *src;
        }

        uint32_t
        getRefCount(void) const
        {
            uint32_t *src = reinterpret_cast<uint32_t *>(_data) + 1;
            return *src;
        }

        void
        incRefCount(void)
        {
            uint32_t *dst = reinterpret_cast<uint32_t *>(_data) + 1;
            ++(*dst);
        }

        void
        decRefCount(void)
        {
            uint32_t *dst = reinterpret_cast<uint32_t *>(_data) + 1;
            --(*dst);
        }

        void
        setEnum(uint32_t enumValue)
        {
            uint32_t *dst = reinterpret_cast<uint32_t *>(_data);
            *dst = enumValue;
        }

        void
        setRefCount(uint32_t refCount)
        {
            uint32_t *dst = reinterpret_cast<uint32_t *>(_data) + 1;
            *dst = refCount;
        }

        static uint32_t size() { return 2*sizeof(uint32_t); }
    };

    typedef std::set<Index, CompareEnumIndex> IndexSet;

protected:

    class EnumBufferType : public datastore::BufferType<char> {
    private:
        uint64_t _minSizeNeeded; // lower cap for sizeNeeded
        uint64_t _deadElems;     // dead elements in active buffer
        bool _pendingCompact;
        bool _wantCompact;
    public:
        EnumBufferType();

        virtual size_t calcClustersToAlloc(uint32_t bufferId, size_t sizeNeeded, bool resizing) const override;

        void setSizeNeededAndDead(uint64_t sizeNeeded, uint64_t deadElems) {
            _minSizeNeeded = sizeNeeded;
            _deadElems = deadElems;
        }

        virtual void
        onFree(size_t usedElems)
        {
            datastore::BufferType<char>::onFree(usedElems);
            _pendingCompact = _wantCompact;
            _wantCompact = false;
        }

        void
        setWantCompact(void)
        {
            _wantCompact = true;
        }

        bool
        getPendingCompact(void) const
        {
            return _pendingCompact;
        }

        void
        clearPendingCompact(void)
        {
            _pendingCompact = false;
        }
    };

    EnumStoreDictBase    *_enumDict;
    DataStoreType         _store;
    EnumBufferType        _type;
    uint32_t              _nextEnum;
    IndexVector           _indexMap;
    std::vector<uint32_t> _toHoldBuffers; // used during compaction
    // set before backgound flush, cleared during background flush
    mutable std::atomic<bool> _disabledReEnumerate;

    static const uint32_t TYPE_ID = 0;

    EnumStoreBase(uint64_t initBufferSize,
                  bool hasPostings);

    virtual ~EnumStoreBase();

    EntryBase getEntryBase(Index idx) const {
        return EntryBase(const_cast<DataStoreType &>(_store).getBufferEntry<char>(idx.bufferId(), idx.offset()));
    }
    datastore::BufferState & getBuffer(uint32_t bufferIdx) {
        return _store.getBufferState(bufferIdx);
    }
    const datastore::BufferState & getBuffer(uint32_t bufferIdx) const {
        return _store.getBufferState(bufferIdx);
    }
    bool validIndex(Index idx) const {
        return (idx.valid() && idx.offset() < _store.getBufferState(idx.bufferId()).size());
    }

    uint32_t getBufferIndex(datastore::BufferState::State status);
    void postCompact(uint32_t newEnum);
    bool preCompact(uint64_t bytesNeeded);

public:
    void
    reset(uint64_t initBufferSize);

    virtual uint32_t getFixedSize() const = 0;
    size_t getMaxEnumOffset() const {
        return _store.getBufferState(_store.getActiveBufferId(TYPE_ID)).size();
    }
    void getEnumValue(const EnumHandle * v, uint32_t *e, uint32_t sz) const;
    uint32_t getRefCount(Index idx) const { return getEntryBase(idx).getRefCount(); }
    uint32_t getEnum(Index idx)     const { return getEntryBase(idx).getEnum(); }
    void incRefCount(Index idx)           { getEntryBase(idx).incRefCount(); }
    void decRefCount(Index idx)           { getEntryBase(idx).decRefCount(); }
    
    // Only use when reading from enumerated attribute save files
    void
    fixupRefCount(Index idx, uint32_t refCount)
    {
        getEntryBase(idx).setRefCount(refCount);
    } 
    
    template <typename Tree>
    void
    fixupRefCounts(const EnumVector &hist,
                   Tree &tree);

    void clearIndexMap()                  { IndexVector().swap(_indexMap); }
    uint32_t getLastEnum()          const { return _nextEnum ? _nextEnum - 1 : _nextEnum; }

    inline uint32_t
    getNumUniques() const
    {
        return _enumDict->getNumUniques();
    }

    uint32_t getRemaining() const {
        return _store.getBufferState(_store.getActiveBufferId(TYPE_ID)).remaining();
    }
    MemoryUsage getMemoryUsage() const;

    inline MemoryUsage
    getTreeMemoryUsage() const
    {
        return _enumDict->getTreeMemoryUsage();
    }

    AddressSpace getAddressSpaceUsage() const;

    bool getCurrentIndex(Index oldIdx, Index & newIdx) const;

    void
    transferHoldLists(generation_t generation);

    void trimHoldLists(generation_t firstUsed);

    static void failNewSize(uint64_t minNewSize, uint64_t maxSize);

    // Align buffers and entries to 4 bytes boundary.
    static uint64_t alignBufferSize(uint64_t val) {
        return Index::align(val);
    }
    static uint32_t alignEntrySize(uint32_t val) {
        return Index::align(val);
    }

    void
    fallbackResize(uint64_t bytesNeeded);

    bool
    getPendingCompact(void) const
    {
        return _type.getPendingCompact();
    }

    void
    clearPendingCompact(void)
    {
        _type.clearPendingCompact();
    }

    template <typename Tree>
    void
    reEnumerate(const Tree &tree);

    inline void
    reEnumerate(void)
    {
        _enumDict->reEnumerate();
    }

    // Disable reenumeration during compaction.
    void disableReEnumerate() const;

    // Allow reenumeration during compaction.
    void enableReEnumerate() const;

    virtual void writeValues(BufferWriter &writer,
                             const Index *idxs, size_t count) const = 0;

    void writeEnumValues(BufferWriter &writer,
                         const Index *idxs, size_t count) const;

    virtual ssize_t
    deserialize(const void *src, size_t available, size_t &initSpace) = 0;

    virtual ssize_t
    deserialize(const void *src, size_t available, Index &idx) = 0;

    virtual bool
    foldedChange(const Index &idx1, const Index &idx2) = 0;

    ssize_t
    deserialize0(const void *src, size_t available, IndexVector &idx);

    template <typename Tree>
    ssize_t
    deserialize(const void *src, size_t available, IndexVector &idx,
                Tree &tree);

    inline ssize_t
    deserialize(const void *src, size_t available,
                IndexVector &idx)
    {
        return _enumDict->deserialize(src, available, idx);
    }

    virtual void
    freeUnusedEnum(Index idx, IndexSet &unused) = 0;

    virtual void
    freeUnusedEnums(bool movePostingIdx) = 0;

    virtual void
    freeUnusedEnums(const IndexVector &toRemove) = 0;

    inline void
    fixupRefCounts(const EnumVector &hist)
    {
        _enumDict->fixupRefCounts(hist);
    }

    inline void
    freezeTree(void)
    {
        _enumDict->freezeTree();
    }

    virtual bool
    performCompaction(uint64_t bytesNeeded) = 0;

    EnumStoreDictBase &
    getEnumStoreDict(void)
    {
        return *_enumDict;
    }

    const EnumStoreDictBase &
    getEnumStoreDict(void) const
    {
        return *_enumDict;
    }

    EnumPostingTree &
    getPostingDictionary(void)
    {
        return _enumDict->getPostingDictionary();
    }

    const EnumPostingTree &
    getPostingDictionary(void) const
    {
        return _enumDict->getPostingDictionary();
    }
};


vespalib::asciistream & operator << (vespalib::asciistream & os, const EnumStoreBase::Index & idx);


/**
 * Base comparator class needed by the btree.
 **/
class EnumStoreComparator {
public:
    typedef EnumStoreBase::Index EnumIndex;
    virtual ~EnumStoreComparator() {}
    /**
     * Compare the values represented by the given enum indexes.
     * Uses the enum store to map from enum index to actual value.
     **/
    virtual bool operator() (const EnumIndex & lhs, const EnumIndex & rhs) const = 0;
};


class EnumStoreComparatorWrapper
{
    const EnumStoreComparator &_comp;
public:
    typedef EnumStoreBase::Index EnumIndex;
    EnumStoreComparatorWrapper(const EnumStoreComparator &comp)
        : _comp(comp)
    {
    }

    inline bool operator()(const EnumIndex &lhs, const EnumIndex &rhs) const
    {
        return _comp(lhs, rhs);
    }
};

extern template class
datastore::DataStoreT<datastore::AlignedEntryRefT<31, 4> >;

}

