#include "../include/cognitive_complexity.h"
#include "../include/builders/c_gsg_builder.h"
#include "../include/builders/javascript_gsg_builder.h"
#include "../include/builders/python_gsg_builder.h"
#include "../include/gsg.h"

static inline LineComplexity build_line_complexity_from_loc(
  const SourceLoc& loc, unsigned int c) {
  return LineComplexity{loc.row, loc.start_col, loc.end_col, c};
}

std::pair<unsigned int, std::vector<LineComplexity>>
compute_cognitive_complexity_gsg(const GSGNode& node, int nesting_level) {
  unsigned int complexity = 0;
  std::vector<LineComplexity> lines;

  auto add = [&](unsigned int c) {
    if (!c) return;
    complexity += c;
    lines.push_back(build_line_complexity_from_loc(node.loc, c));
  };
  auto count_children = [&](int body_nesting) {
    for (const auto& ch : node.children) {
      int next =
        (ch.kind == GSGNodeKind::Header) ? nesting_level : body_nesting;
      auto [c, l] = compute_cognitive_complexity_gsg(ch, next);
      complexity += c;
      if (!l.empty()) lines.insert(lines.end(), l.begin(), l.end());
    }
  };
  const auto level = static_cast<unsigned int>(nesting_level);

  switch (node.kind) {
    case GSGNodeKind::For:
    case GSGNodeKind::While:
    case GSGNodeKind::DoWhile:
    case GSGNodeKind::If:
    case GSGNodeKind::Except:
    case GSGNodeKind::Catch:
      add(1 + level + node.addl_cost);
      count_children(nesting_level + 1);
      break;
    case GSGNodeKind::ElseIf:
    case GSGNodeKind::Else:
      add(1 + node.addl_cost);
      count_children(nesting_level + 1);
      break;
    case GSGNodeKind::Switch:
    case GSGNodeKind::Ternary:
      add(1 + level + node.addl_cost);
      count_children(nesting_level);
      break;
    case GSGNodeKind::Function:
    case GSGNodeKind::Case:
      add(node.addl_cost);
      count_children(nesting_level + 1);
      break;
    default:
      add(node.addl_cost);
      count_children(nesting_level);
      break;
  }

  return {complexity, lines};
}

std::unique_ptr<IBuilder> make_builder(Language lang) {
  switch (lang) {
    case Language::Python:
      return std::make_unique<PythonGSGBuilder>();
    case Language::JavaScript:
      return std::make_unique<JavaScriptGSGBuilder>();
    case Language::TypeScript:
      return std::make_unique<JavaScriptGSGBuilder>();
    case Language::C:
    case Language::Cpp:
      return std::make_unique<CLikeGSGBuilder>();
    default:
      return nullptr;
  }
}

std::vector<FunctionComplexity> functions_complexity_file(
  const std::string& source_code, TSParser* parser, Language lang) {
  std::vector<FunctionComplexity> functions;

  TSTree* tree = ts_parser_parse_string(parser, nullptr, source_code.c_str(),
                                        strlen(source_code.c_str()));
  TSNode root_node = ts_tree_root_node(tree);

  auto builder = make_builder(lang);
  if (!builder) {
    ts_tree_delete(tree);
    return functions;
  }

  auto func_nodes = builder->build_functions(root_node, source_code);
  for (const auto& fn : func_nodes) {
    unsigned int c = 0;
    std::vector<LineComplexity> lines;
    for (const auto& ch : fn.children) {
      auto [cc, l] = compute_cognitive_complexity_gsg(ch, 0);
      c += cc;
      lines.insert(lines.end(), l.begin(), l.end());
    }
    functions.push_back(FunctionComplexity{.name = fn.name,
                                           .complexity = c,
                                           .row = fn.loc.row,
                                           .start_col = fn.loc.start_col,
                                           .end_col = fn.loc.end_col,
                                           .lines = lines});
  }

  ts_tree_delete(tree);
  return functions;
}
