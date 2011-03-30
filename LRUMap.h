#ifndef LRUMAP_H
#define LRUMAP_H

#include <tr1/unordered_map>
#include <list>

template <typename Key, typename Value>
class LRUMap {
public:
	typedef size_t Weight;
	
	struct Entry {
		Key key;
		Value value;
		Weight weight;
		
		Entry(Key k, Value v, Weight w) : key(k), value(v), weight(w) { }
	};

private:
	typedef std::list<Entry> LRUList; // most-recent at front
	typedef typename LRUList::iterator LRUIterator;
	typedef std::tr1::unordered_map<Key, LRUIterator> IterMap;

public:
	typedef LRUIterator Iterator;

private:	
	LRUList mLRU;
	IterMap mMap;
	Weight mWeight, mMaxWeight;
	
	void makeRoom(Weight newWeight) {
		for (LRUIterator uiter = --mLRU.end(); mWeight > newWeight; --uiter) {
			Weight w = uiter->weight;
			mMap.erase(uiter->key);
			mLRU.erase(uiter);
			mWeight -= w;
		}
	}
	
	void markNew(const LRUIterator& iter) {
		mLRU.splice(mLRU.begin(), mLRU, iter);
	}
	
public:
	LRUMap(Weight maxWeight) : mWeight(), mMaxWeight(maxWeight) { }
	
	Weight weight() const { return mWeight; }
	
	Weight maxWeight() const { return mMaxWeight; }
	void maxWeight(Weight w) { makeRoom(w); mMaxWeight = w; }
	
	// Doesn't change LRU-time
	Iterator begin() { return mLRU.begin(); }
	Iterator end() { return mLRU.end(); }
	
	// Add a new item, ejecting old items to make room if necessary
	Entry& add(Key k, Value v, Weight w) {
		makeRoom(maxWeight() - w);
		mWeight += w;
		mLRU.push_front(Entry(k, v, w));
		mMap[k] = mLRU.begin();
		return mLRU.front();
	}
	
	// Find an item, returning null-ptr if not found
	Value *find(Key k) {
		typename IterMap::iterator miter = mMap.find(k);
		if (miter == mMap.end())
			return 0;
		
		markNew(miter->second);
		return &miter->second->value;
	}
};

#endif // LRUMAP_H
