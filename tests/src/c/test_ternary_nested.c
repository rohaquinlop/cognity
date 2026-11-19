int f(int a, int b, int c) {
  return a ? (b ? 1 : 2) : 3;
}

int g(int a, int b, int c) {
  return (a ? b : c) ? 1 : 2;
}
