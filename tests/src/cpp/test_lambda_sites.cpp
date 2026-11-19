int f(int a) {
  if (a) {
  } else if ([]() { if (true) return 1; return 0; }()) {
  }
  throw []() { if (true) return 1; return 0; }();
}
