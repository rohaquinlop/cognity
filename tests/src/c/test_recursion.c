int fact(int n) {
  if (n <= 1) {
    return 1;
  }
  return n * fact(n - 1);
}

int is_even(int n);

int is_odd(int n) {
  return n == 0 ? 0 : is_even(n - 1);
}

int is_even(int n) {
  return n == 0 ? 1 : is_odd(n - 1);
}

int no_args(void) {
  return no_args();
}

int apply(int (*apply)(int)) {
  return apply(0);
}

int local_shadow(int n) {
  int (*local_shadow)(int) = 0;
  return local_shadow(n);
}

int f1(int n) {
  int (*f1)(int);
  f1 = 0;
  return f1(n);
}

int *f2(int n) {
  int *f2(int);
  return f2(n);
}
