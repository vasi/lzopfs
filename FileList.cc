#include "FileList.h"

#include <cstdio>

#include "LzopFile.h"
#include "PixzFile.h"
#include "GzipFile.h"
#include "Bzip2File.h"

const FileList::OpenerList FileList::Openers(initOpeners());

FileList::OpenerList FileList::initOpeners() {
	OpenFunc o[] = { LzopFile::open, GzipFile::open, Bzip2File::open,
		PixzFile::open };
	return OpenerList(o, o + sizeof(o)/sizeof(o[0]));
}


CompressedFile *FileList::find(const std::string& dest) {
	Map::iterator found = mMap.find(dest);
	if (found == mMap.end())
		return 0;
	return found->second;
}

void FileList::add(const std::string& source) {
	CompressedFile *file = 0;
	try {
		OpenerList::const_iterator iter;
		for (iter = Openers.begin(); iter != Openers.end(); ++iter) {
			try {
				file = (*iter)(source, mMaxBlockSize);
				break;
			} catch (CompressedFile::FormatException& e) {
				// just keep going
			}
		}
		if (!file) {
			fprintf(stderr, "Don't understand format of file %s, skipping.\n",
				source.c_str());
			return;
		}
		
		std::string dest("/");
		dest.append(file->destName());
		mMap[dest] = file;		
	} catch (std::runtime_error& e) {
		fprintf(stderr, "Error reading file %s, skipping: %s\n",
			source.c_str(), e.what());
	}
}

FileList::~FileList() {
	for (Map::iterator iter = mMap.begin(); iter != mMap.end(); ++iter) {
		delete iter->second;
	}
}
