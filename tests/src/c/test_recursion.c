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
