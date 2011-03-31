#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <string>

namespace PathUtils {
	std::string basename(const std::string& path);
	std::string removeExtension(const std::string& name,
		const std::string& suffix);
}

#endif // PATHUTILS_H
