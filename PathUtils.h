#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <string>

namespace PathUtils {
	size_t endsWith(const std::string& haystack, const std::string& needle);
	
	std::string basename(const std::string& path);
	size_t hasExtension(const std::string& name, const std::string& ext);
	bool removeExtension(std::string& name, const std::string& ext);
	bool replaceExtension(std::string& name, const std::string& ext,
		const std::string& replace);
	
	std::string realpath(const std::string& path);
}

#endif // PATHUTILS_H
