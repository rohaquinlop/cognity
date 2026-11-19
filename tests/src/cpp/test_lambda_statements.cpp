void f(int* v) {
  for (int x : {1, 2}) {
    auto g = []() { if (true) return 1; return 0; };
  }
  done: auto h = []() { if (true) return 1; return 0; };
}
