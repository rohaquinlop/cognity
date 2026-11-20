#pragma once

#include <tree_sitter/api.h>

#ifdef __linux__
#include <cstring>
#endif

#include <string>
#include <vector>

#include "./gsg.h"

void collect_source_files(const std::vector<std::string>& inputs,
                          const std::vector<Language>& filter,
                          const std::vector<std::string>& excludes,
                          std::vector<std::string>& out);
