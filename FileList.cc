#include "FileList.h"

LzopFile *FileList::find(const std::string& dest) {
	Map::iterator found = mMap.find(dest);
	if (found == mMap.end())
		return 0;
	return found->second;
}

void FileList::add(const std::string& source) {
	LzopFile *lzop = new LzopFile(source);
	std::string dest("/");
	dest.append(lzop->destName());
	mMap[dest] = lzop;
}

FileList::~FileList() {
	for (Map::iterator iter = mMap.begin(); iter != mMap.end(); ++iter) {
		delete iter->second;
	}
}
