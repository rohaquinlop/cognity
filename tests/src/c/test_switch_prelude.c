int f(int x) {
  switch (x) {
    if (x) {}
    case 1:
      break;
  }
  return 0;
}

int g(int x, int a, int b) {
  switch (x) {
    // comment before the first label
    while (a && b) {}
    x = a || b;
    default:
      if (x) {}
  }
  return 0;
}
