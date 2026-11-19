int mixed(int a, int b, int c, int d) {
  return a && b || c && d;
}

int not_operand(int a, int b) {
  return !(a && b);
}

int not_alone(int a) {
  return !a;
}

int ne(int a, int b) {
  if (a != b) {
    return 1;
  }
  return 0;
}

int for_header(int n, int a) {
  for (int i = 0; i < n && a; i++) {
    a--;
  }
  return a;
}

int switch_cond(int a, int b) {
  switch (a || b) {
    case 1:
      return 1;
  }
  return 0;
}
