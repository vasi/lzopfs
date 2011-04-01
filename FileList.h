#ifndef FILELIST_H
#define FILELIST_H

#include "CompressedFile.h"

#include <string>
#include <vector>
#include <tr1/unordered_map>

class FileList {
private:
	// Disable copying
	FileList(const FileList& o);
	FileList& operator=(const FileList& o);
	
protected:
	typedef std::tr1::unordered_map<std::string,CompressedFile*> Map;
	Map mMap;
	uint64_t mMaxBlockSize;
	
	typedef CompressedFile* (*OpenFunc)(const std::string& path,
		uint64_t maxBlock);
	typedef std::vector<OpenFunc> OpenerList;
	static const OpenerList Openers;
	static OpenerList initOpeners();
	
public:
	FileList(uint64_t maxBlockSize = UINT64_MAX)
		: mMaxBlockSize(maxBlockSize) { }
	virtual ~FileList();
	
	CompressedFile *find(const std::string& dest);
	void add(const std::string& source);
	
	template <typename Op>
	void forNames(Op op) {
		Map::const_iterator iter;
		for (iter = mMap.begin(); iter != mMap.end(); ++iter)
			op(iter->first);
	}
};

#endif // FILELIST_H
