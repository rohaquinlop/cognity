void thrower(bool a, bool b) {
  throw a || b;
}

bool alt_tokens(bool a, bool b, bool c) {
  return a and b or c;
}

bool not_token(bool a, bool b) {
  return not (a and b);
}
