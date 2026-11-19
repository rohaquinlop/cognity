#include "../include/recursion.h"

#include <algorithm>
#include <limits>

namespace {

constexpr std::size_t kUnset = std::numeric_limits<std::size_t>::max();

struct Frame {
  std::size_t def;
  std::size_t next;
};

std::vector<std::size_t> strongly_connected_components(
  const std::vector<std::vector<CallEdge>>& edges) {
  const std::size_t n = edges.size();
  std::vector<std::size_t> index(n, kUnset), low(n, 0), component(n, kUnset);
  std::vector<bool> on_stack(n, false);
  std::vector<std::size_t> stack;
  std::vector<Frame> frames;
  std::size_t counter = 0;
  auto visit = [&](std::size_t v) {
    index[v] = low[v] = counter++;
    stack.push_back(v);
    on_stack[v] = true;
    frames.push_back({.def = v, .next = 0});
  };
  for (std::size_t root = 0; root < n; ++root) {
    if (index[root] != kUnset) continue;
    visit(root);
    while (!frames.empty()) {
      const std::size_t v = frames.back().def;
      if (frames.back().next < edges[v].size()) {
        const std::size_t w = edges[v][frames.back().next++].callee;
        if (index[w] == kUnset)
          visit(w);
        else if (on_stack[w])
          low[v] = std::min(low[v], index[w]);
        continue;
      }
      if (low[v] == index[v]) {
        std::size_t w = kUnset;
        while (w != v) {
          w = stack.back();
          stack.pop_back();
          on_stack[w] = false;
          component[w] = v;
        }
      }
      frames.pop_back();
      if (!frames.empty()) {
        const std::size_t parent = frames.back().def;
        low[parent] = std::min(low[parent], low[v]);
      }
    }
  }
  return component;
}

}  // namespace

std::vector<RecursiveDef> recursive_defs_from_graph(
  const std::vector<uint32_t>& def_starts,
  const std::vector<std::vector<CallEdge>>& edges) {
  const auto component = strongly_connected_components(edges);
  std::vector<std::size_t> sizes(edges.size(), 0);
  for (std::size_t c : component) ++sizes[c];
  std::vector<RecursiveDef> out;
  for (std::size_t id = 0; id < edges.size(); ++id) {
    const std::size_t c = component[id];
    for (const auto& e : edges[id]) {
      if (component[e.callee] == c && (sizes[c] > 1 || e.callee == id)) {
        out.push_back({.def_start_byte = def_starts[id], .call = e.call});
        break;
      }
    }
  }
  return out;
}
