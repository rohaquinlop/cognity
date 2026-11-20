int r_if(int a) {
  if (a) {
    return 1;
  }
  return 0;
}

int r_else_if(int a, int b) {
  if (a) {
    return 1;
  } else if (b) {
    return 2;
  }
  return 0;
}

int r_else(int a) {
  if (a) {
    return 1;
  } else {
    return 2;
  }
}

int r_nest_in_else(int a, int b) {
  if (a) {
    return 1;
  } else {
    if (b) {
      return 2;
    }
  }
  return 0;
}

int r_for(int n) {
  for (int i = 0; i < n; i++) {
    if (i) {
      return i;
    }
  }
  return 0;
}

void r_while(int a) {
  while (a) {
    a--;
  }
}

void r_do_while(int a) {
  do {
    a--;
  } while (a);
}

int r_switch(int a) {
  switch (a) {
    case 1:
      if (a) {
        return 1;
      }
      break;
    default:
      return 0;
  }
  return 0;
}

int r_ternary(int a) {
  return a ? 1 : 2;
}

int r_ternary_nested(int a, int b) {
  return a ? (b ? 1 : 2) : 3;
}

int r_bool_seq(int a, int b, int c, int d) {
  return a && b && c || d;
}

int r_not(int a, int b) {
  return !(a && b);
}

int r_recursion(int n) {
  return r_recursion(n - 1);
}

void r_break_continue(int n) {
  for (int i = 0; i < n; i++) {
    if (i) {
      continue;
    }
    break;
  }
}

int r_try_catch(int a) {
  try {
    if (a) {
      return 1;
    }
  } catch (int e) {
    if (e) {
      return 2;
    }
  }
  return 0;
}

int r_lambda_nesting(int a) {
  auto f = [&]() {
    if (a) {
      return 1;
    }
    return 0;
  };
  return f();
}

int r_range_for(int* xs) {
  for (int x : {1, 2}) {
    if (x) {
      return x;
    }
  }
  return 0;
}

struct K {
  int m(int a) {
    if (a) {
      return 1;
    }
    return 0;
  }
};

int r_labeled_while(int x) {
start:
  while (x) {
    x--;
  }
  return 0;
}

int r_bool_in_declaration(int a, int b) {
  int x = a && b;
  return x;
}

void r_local_struct() {
  struct L {
    int m(int x) {
      if (x) {
        return 1;
      }
      return 0;
    }
  };
}

int r_lambda_in_ternary(int a, int b) {
  return a ? [&]() {
    if (b) {
      return 2;
    }
    return 1;
  }() : 0;
}

void r_goto(int x) {
  if (x) {
    goto end;
  }
  x++;
end:
  return;
}

struct P {
  int m(int x) {
#ifndef NDEBUG
    for (int i = 0; i < x; i++) {
    }
#endif
    return 0;
  }
};
