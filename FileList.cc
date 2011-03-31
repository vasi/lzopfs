#include "FileList.h"

#include "LzopFile.h"

CompressedFile *FileList::find(const std::string& dest) {
	Map::iterator found = mMap.find(dest);
	if (found == mMap.end())
		return 0;
	return found->second;
}

void FileList::add(const std::string& source) {
	CompressedFile *file = new LzopFile(source);
	std::string dest("/");
	dest.append(file->destName());
	mMap[dest] = file;
}

FileList::~FileList() {
	for (Map::iterator iter = mMap.begin(); iter != mMap.end(); ++iter) {
		delete iter->second;
	}
}
