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
	size_t blockFactor;

	OpenParams(uint64_t pMaxBlock, std::string pIndexRoot, size_t pBlockFactor)
		: maxBlock(pMaxBlock), indexRoot(pIndexRoot), blockFactor(pBlockFactor) {}
};

class FileList {
private:
	// Disable copying
	FileList(const FileList& o);
	FileList& operator=(const FileList& o);
	
protected:
	typedef unordered_map<std::string,CompressedFile*> Map;
	Map mMap;
	OpenParams mOpenParams;
	
	typedef CompressedFile* (*OpenFunc)(const std::string& path,
		const OpenParams& params);
	typedef std::vector<OpenFunc> OpenerList;
	static const OpenerList Openers;
	static OpenerList initOpeners();
	
public:
	FileList(OpenParams params)
		: mOpenParams(params)
 {
		if (!mOpenParams.indexRoot.empty())
			mOpenParams.indexRoot = PathUtils::realpath(mOpenParams.indexRoot);
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
