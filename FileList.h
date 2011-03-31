#ifndef FILELIST_H
#define FILELIST_H

#include "LzopFile.h"

#include <tr1/unordered_map>

class FileList {
	typedef std::tr1::unordered_map<std::string,LzopFile*> Map;
	Map mMap;
	
public:
	virtual ~FileList();
	
	LzopFile *find(const std::string& dest);
	void add(const std::string& source);
	
	template <typename Op>
	void forNames(Op op) {
		Map::const_iterator iter;
		for (iter = mMap.begin(); iter != mMap.end(); ++iter)
			op(iter->first);
	}
};

#endif // FILELIST_H
