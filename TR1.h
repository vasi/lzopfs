#ifndef TR1_H
#define TR1_H

#ifdef HAS_TR1
	#include <tr1/unordered_map>
	using std::tr1::unordered_map;
	using std::tr1::hash;
	#include <tr1/memory>
	using std::tr1::shared_ptr;
	using std::tr1::unique_ptr;
#elif defined(HAS_BOOST_TR1)
	#include <boost/unordered_map.hpp>
	using boost::unordered_map;
	using boost::hash;
	#include <boost/shared_ptr.hpp>
	using boost::shared_ptr;
	using boost::unique_ptr;
#else
	#include <unordered_map>
	using std::unordered_map;
	using std::hash;
	#include <memory>
	using std::shared_ptr;
	using std::unique_ptr;
#endif

#endif // TR1_H
