#ifndef LANGUAGES_H
#define LANGUAGES_H

#include <tree_sitter/api.h>

#include <memory>
#include <span>
#include <string_view>

#include "./gsg.h"

struct LanguageInfo {
  Language id;
  std::span<const std::string_view> tokens;
  std::span<const std::string_view> extensions;
  const TSLanguage* (*grammar)(std::string_view path);
  std::unique_ptr<IBuilder> (*make_builder)();
};

const LanguageInfo* language_info(Language lang);
Language language_from_token(std::string_view tok);
Language language_from_path(std::string_view path);

#endif
