#ifndef COGNITIVE_COMPLEXITY_H
#define COGNITIVE_COMPLEXITY_H

#ifdef __linux__
#include <cstring>
#endif

#include <tree_sitter/api.h>

#include <vector>

#include "./gsg.h"

struct LineComplexity {
  unsigned int row;
  unsigned int start_col;
  unsigned int end_col;
  unsigned int complexity;
};

struct FunctionComplexity {
  std::string name;
  unsigned int complexity;
  unsigned int row;
  unsigned int start_col;
  unsigned int end_col;
  std::vector<LineComplexity> lines;
};

struct FileComplexity {
  std::string path;
  std::string file_name;
  std::vector<FunctionComplexity> functions;
};

struct CodeComplexity {
  std::vector<FunctionComplexity> functions;
};

std::vector<FunctionComplexity> functions_complexity_file(const std::string&,
                                                          TSParser*, Language);

std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity_gsg(const GSGNode&, int);

#endif
