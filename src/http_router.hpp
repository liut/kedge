#pragma once


#include <algorithm>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace btd {
namespace web {


struct Path {
    std::regex re;
    std::vector<std::string> keys;
};

class Match;

class Route {

	friend class Match;

private:
	std::string uri;
	std::regex path_to_regex;

    //
    // a pattern used to replace the url template's "captures".
    //
	static const std::string capture_pattern = "(?:([^\\/]+?))";

    //
    // a pattern for finding "captures" in a url template.
    //
	static const std::string path_pattern = ":([^\\/]+)?";

    //
    // cache each regex and its keys as it is created by the
    // test method.
    //
	std::map<std::string, Path> cache;

public:

	// prepare for every request with uri
	Match
	to(const std::string& u)
	{
		uri = u;
		path_to_regex = std::regex(path_pattern);
		Match m(*this);
		return m;
	}
};

class Match {

private:
	Route *route;
	std::map<std::string, std::string> pairs;
	int keys;

public:
	std::optional<std::string>
	get(const std::string& key)
	{
		if (pairs.count(key)) {
			return pairs.at(key);
		}
		return std::nullopt;
	}

	bool
	test(const std::string& tmpl)
	{
		pairs.clear();
		Path path;
		keys = 0;

		if (route->cache.count(tmpl)) {
			path = route->cache.at(tmpl);
		}
		else
		{
	      //
	      // get all the keys from the path.
	      //
			std::sregex_token_iterator
			i(tmpl.begin(), tmpl.end(), route->path_to_regex),
			iend;

			while(i != iend) {
				std::string key = *i++;
				path.keys.push_back(key.erase(0, 1));
			}

	      //
	      // create a regex from the path.
	      //
			auto exp = std::regex_replace(tmpl, route->path_to_regex, Route::capture_pattern );
			path.re = std::regex("^" + exp + "$");
		}

		std::smatch sm_values;
		if (!std::regex_match(route->uri, sm_values, path.re)) {
			return false;
		}

		if (sm_values.empty()) {
			return true;
		}

		for (auto i = 0; i < sm_values.size() - 1; i++) {
			std::string key = path.keys[i];
			pairs.insert(std::pair<std::string, std::string>(key, sm_values[i + 1]));
			keys++;
		}
		return true;
	}

	Match(Route &r) : route(&r){}
};

} // namespace web
} // namespace btd
