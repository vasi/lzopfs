#ifndef FILELIST_H
#define FILELIST_H

#include "CompressedFile.h"
#include "TR1.h"
#include "PathUtils.h"

#include <string>
#include <vector>

#include <stdint.h>

struct OpenParams {
	uint64_t maxBlock;
	std::string indexRoot;

	OpenParams(uint64_t pMaxBlock, std::string pIndexRoot) : maxBlock(pMaxBlock), indexRoot(pIndexRoot) {}
};

class FileList {
private:
	// Disable copying
	FileList(const FileList& o);
	FileList& operator=(const FileList& o);

protected:
	typedef unordered_map<std::string,CompressedFile*> Map;
	Map mMap;
	uint64_t mMaxBlockSize;
	std::string mIndexRoot;

	typedef CompressedFile* (*OpenFunc)(const std::string& path,
		const OpenParams& params);
	typedef std::vector<OpenFunc> OpenerList;
	static const OpenerList Openers;
	static OpenerList initOpeners();

public:
	FileList(uint64_t maxBlockSize, const char *indexRoot)
		: mMaxBlockSize(maxBlockSize) {
		if (indexRoot != NULL)
			mIndexRoot = PathUtils::realpath(indexRoot);
	}
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
