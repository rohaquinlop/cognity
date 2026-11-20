int f(int* v, int a, int b) {
  for (;;) if (v) {}
  while (v) if (v) {}
  do if (v) {} while (v);
  if (v) if (v) {}
  if (v) return a && b;
  for (;;) a = a && b;
  return 0;
}
