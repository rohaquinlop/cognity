function r_if(a) {
  if (a) {
    return 1;
  }
}

function r_else_if(a, b) {
  if (a) {
    return 1;
  } else if (b) {
    return 2;
  }
}

function r_else(a) {
  if (a) {
    return 1;
  } else {
    return 2;
  }
}

function r_nest_in_else(a, b) {
  if (a) {
    return 1;
  } else {
    if (b) {
      return 2;
    }
  }
}

function r_for(xs) {
  for (const x of xs) {
    if (x) {
      return x;
    }
  }
}

function r_while(a) {
  while (a) {
    a--;
  }
}

function r_do_while(a) {
  do {
    a--;
  } while (a);
}

function r_switch(a) {
  switch (a) {
    case 1:
      if (a) {
        return 1;
      }
      break;
    default:
      return 0;
  }
}

function r_try_catch_finally(a) {
  try {
    if (a) {
      return 1;
    }
  } catch (e) {
    if (e) {
      return 2;
    }
  } finally {
    if (a) {
      a = 0;
    }
  }
}

function r_ternary(a) {
  return a ? 1 : 2;
}

function r_ternary_nested(a, b) {
  return a ? (b ? 1 : 2) : 3;
}

function r_bool_seq(a, b, c, d) {
  return a && b && c || d;
}

function r_not(a, b) {
  return !(a && b);
}

function r_arrow_nesting(xs) {
  return xs.map((x) => {
    if (x) {
      return 1;
    }
  });
}

function r_nested_function(a) {
  function inner() {
    if (a) {
      return 1;
    }
  }
  return inner();
}

function r_recursion(n) {
  return r_recursion(n - 1);
}

function r_break_continue(xs) {
  for (const x of xs) {
    if (x) {
      continue;
    }
    break;
  }
}

class K {
  m(a) {
    if (a) {
      return 1;
    }
  }
}
