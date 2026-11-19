#ifndef RECURSION_H
#define RECURSION_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "./gsg.h"

struct CallEdge {
  std::size_t callee;
  SourceLoc call;
};

/// Each def in a recursion cycle (a component of size > 1, or a self call),
/// with its first call, in edge order, into its own component.
std::vector<RecursiveDef> recursive_defs_from_graph(
  const std::vector<uint32_t>& def_starts,
  const std::vector<std::vector<CallEdge>>& edges);

#endif
