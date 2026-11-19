void f(int* v) {
  for (int x : {1, 2}) {
    if (x) {}
  }
}

void g(int* v, bool a, bool b) {
  for (auto& [k, w] : a && b ? m : n) {
    for (int y : v) {
      if (y) {}
    }
  }
}

void h(int* v) {
  for (int i = 0; auto x : [](){ if (1) {} return xs; }()) {}
}
