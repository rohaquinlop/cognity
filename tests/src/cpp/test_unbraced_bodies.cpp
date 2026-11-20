void f(int* v) {
  for (int x : {1, 2}) if (x) {}
  if (v) g([]() { if (true) {} });
}
