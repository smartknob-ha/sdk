#ifndef SEMANTIC_VERSIONING_HPP
#define SEMANTIC_VERSIONING_HPP

#include "etl/string.h"
#include "etl/to_string.h"
#include "semver/include/semver.hpp"

namespace semver {

    template<size_t LENGTH = semver::max_version_string_length>
    version from_string(etl::string<LENGTH> string) {
        return version(string.c_str());
    }

    template<size_t LENGTH = semver::max_version_string_length>
    etl::string<LENGTH> to_etl_string(version v) {
        etl::string<LENGTH> string;

        etl::to_string(v.major, string, true);
        string.append(".");
        etl::to_string(v.minor, string, true);
        string.append(".");
        etl::to_string(v.patch, string, true);
        if (v.prerelease_number) {
            string.append("-rc.");
            etl::to_string(v.prerelease_number.value(), string, true);
        }
        return string;
    }

} // namespace semver

#endif // SEMANTIC_VERSIONING_HPP
