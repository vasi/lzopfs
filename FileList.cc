#include "FileList.h"

#include <cstdio>

#ifdef HAVE_LZO
#include "LzopFile.h"
#endif
#ifdef HAVE_ZLIB
#include "GzipFile.h"
#endif
#ifdef HAVE_BZIP2
#include "Bzip2File.h"
#endif
#ifdef HAVE_LZMA
#include "PixzFile.h"
#endif
#ifdef HAVE_ZSTD
#include "ZstdFile.h"
#endif

const FileList::OpenerList FileList::Openers(initOpeners());

FileList::OpenerList FileList::initOpeners() {
	OpenFunc o[] = {
#ifdef HAVE_LZO
		LzopFile::open,
#endif
#ifdef HAVE_ZLIB
		GzipFile::open,
#endif
#ifdef HAVE_BZIP2
		Bzip2File::open,
#endif
#ifdef HAVE_LZMA
		PixzFile::open,
#endif
#ifdef HAVE_ZSTD
		ZstdFile::open,
#endif
	};
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
				file = (*iter)(source, mOpenParams);
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
