#include "PathUtils.h"
#include <stdlib.h>
using std::string;

namespace PathUtils {

string basename(const string& path) {
	if (path.empty())
		return string(".");
	
	size_t bend = path.find_last_not_of('/');
	if (bend == string::npos)
		return string("/");
	
	size_t bstart = path.find_last_of('/', bend);
	return string(
		&path[bstart == string::npos ? 0 : bstart + 1],
		&path[bend + 1]
	);
}

size_t endsWith(const string& haystack, const string& needle) {
	if (haystack.size() < needle.size())
		return string::npos;
	if (haystack.compare(haystack.size() - needle.size(),
			needle.size(), needle) != 0)
		return string::npos;
	
	return haystack.size() - needle.size();
}

size_t hasExtension(const string& name, const string& ext) {
	string e(".");
	e.append(ext);
	
	size_t pos = endsWith(name, e);
	if (pos == 0 || pos == string::npos) // can't start with extension
		return string::npos;
	return pos + 1; // start of extension
}

bool replaceExtension(string& name, const string& ext,
		const string& replace) {
	size_t pos = hasExtension(name, ext);
	if (pos == string::npos)
		return false;
	name.replace(pos, name.size() - pos, replace);
	return true;
}

bool removeExtension(std::string& name, const std::string& ext) {
	size_t pos = hasExtension(name, ext);
	if (pos == string::npos)
		return false;
	name.resize(pos - 1);
	return true;
}

string realpath(const string& path) {
	char *abs = 0;
	try {
		abs = ::realpath(path.c_str(), NULL);
		string ret(abs);
		free(abs);
		return ret;
	} catch (...) {
		if (abs) free(abs);
		throw;
	}
}

}
