#include "PathUtils.h"

namespace PathUtils {

std::string basename(const std::string& path) {
	if (path.empty())
		return std::string(".");
	
	size_t bend = path.find_last_not_of('/');
	if (bend == std::string::npos)
		return std::string("/");
	
	size_t bstart = path.find_last_of('/', bend);
	return std::string(
		&path[bstart == std::string::npos ? 0 : bstart + 1],
		&path[bend + 1]
	);
}

std::string removeExtension(const std::string& name,
		const std::string& suffix) {
	if (name.size() <= suffix.size())
		return name;
	
	if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
		return name.substr(0, name.size() - suffix.size());
	return name;
}

}
