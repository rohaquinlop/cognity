int f(int x) {
  switch (x) {
    case 1:
      if (x > 0) return 1;
      break;
    default: {
      auto g = []() { if (true) return 1; return 0; };
      return g();
    }
  }
  return 0;
}
