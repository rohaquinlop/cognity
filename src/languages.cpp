#include "../include/languages.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "../include/builders/c_gsg_builder.h"
#include "../include/builders/javascript_gsg_builder.h"
#include "../include/builders/python_gsg_builder.h"

extern "C" {
const TSLanguage* tree_sitter_python();
const TSLanguage* tree_sitter_javascript();
const TSLanguage* tree_sitter_typescript();
const TSLanguage* tree_sitter_tsx();
const TSLanguage* tree_sitter_c();
const TSLanguage* tree_sitter_cpp();
}

namespace {

constexpr std::array<std::string_view, 2> kPythonTokens = {"py", "python"};
constexpr std::array<std::string_view, 1> kPythonExts = {".py"};
constexpr std::array<std::string_view, 1> kCTokens = {"c"};
constexpr std::array<std::string_view, 1> kCExts = {".c"};
constexpr std::array<std::string_view, 4> kCppTokens = {"cpp", "c++", "cc",
                                                        "cxx"};
constexpr std::array<std::string_view, 3> kCppExts = {".cpp", ".cc", ".cxx"};
constexpr std::array<std::string_view, 2> kJsTokens = {"js", "javascript"};
constexpr std::array<std::string_view, 3> kJsExts = {".js", ".mjs", ".cjs"};
constexpr std::array<std::string_view, 3> kTsTokens = {"ts", "typescript",
                                                       "tsx"};
constexpr std::array<std::string_view, 2> kTsExts = {".ts", ".tsx"};

constexpr std::size_t kLanguageCount =
  static_cast<std::size_t>(Language::Unknown);

constexpr std::array<LanguageInfo, kLanguageCount> kLanguages = {{
  {.id = Language::Python,
   .tokens = kPythonTokens,
   .extensions = kPythonExts,
   .grammar = [](std::string_view) { return tree_sitter_python(); },
   .make_builder = []() -> std::unique_ptr<IBuilder> {
     return std::make_unique<PythonGSGBuilder>();
   }},
  {.id = Language::C,
   .tokens = kCTokens,
   .extensions = kCExts,
   .grammar = [](std::string_view) { return tree_sitter_c(); },
   .make_builder = []() -> std::unique_ptr<IBuilder> {
     return std::make_unique<CLikeGSGBuilder>(false);
   }},
  {.id = Language::Cpp,
   .tokens = kCppTokens,
   .extensions = kCppExts,
   .grammar = [](std::string_view) { return tree_sitter_cpp(); },
   .make_builder = []() -> std::unique_ptr<IBuilder> {
     return std::make_unique<CLikeGSGBuilder>(true);
   }},
  {.id = Language::JavaScript,
   .tokens = kJsTokens,
   .extensions = kJsExts,
   .grammar = [](std::string_view) { return tree_sitter_javascript(); },
   .make_builder = []() -> std::unique_ptr<IBuilder> {
     return std::make_unique<JavaScriptGSGBuilder>();
   }},
  {.id = Language::TypeScript,
   .tokens = kTsTokens,
   .extensions = kTsExts,
   .grammar =
     [](std::string_view path) {
       return path.ends_with(".tsx") ? tree_sitter_tsx()
                                     : tree_sitter_typescript();
     },
   .make_builder = []() -> std::unique_ptr<IBuilder> {
     return std::make_unique<JavaScriptGSGBuilder>();
   }},
}};

consteval bool registry_is_complete() {
  for (std::size_t i = 0; i < kLanguageCount; ++i) {
    const LanguageInfo& info = kLanguages.at(i);
    if (info.id != static_cast<Language>(i) || info.grammar == nullptr ||
        info.make_builder == nullptr || info.tokens.empty() ||
        info.extensions.empty())
      return false;
  }
  return true;
}

static_assert(registry_is_complete(),
              "every Language value needs a full entry in kLanguages");

}  // namespace

const LanguageInfo* language_info(Language lang) {
  const auto i = static_cast<std::size_t>(lang);
  return i < kLanguageCount ? &kLanguages.at(i) : nullptr;
}

Language language_from_token(std::string_view tok) {
  std::string lower(tok);
  std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  for (const LanguageInfo& info : kLanguages)
    if (std::ranges::find(info.tokens, lower) != info.tokens.end())
      return info.id;
  return Language::Unknown;
}

Language language_from_path(std::string_view path) {
  for (const LanguageInfo& info : kLanguages)
    for (std::string_view ext : info.extensions)
      if (path.ends_with(ext)) return info.id;
  return Language::Unknown;
}
