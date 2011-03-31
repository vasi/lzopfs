#ifndef FILELIST_H
#define FILELIST_H

#include "CompressedFile.h"

#include <string>
#include <tr1/unordered_map>

class FileList {
	typedef std::tr1::unordered_map<std::string,CompressedFile*> Map;
	Map mMap;
	
public:
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
