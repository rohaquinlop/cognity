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

function r_unbraced_nested_if(a, c) {
  if (a) if (c) return 1;
  return 0;
}

function r_unbraced_loop_body(x) {
  while (x) if (x > 1) x--;
}

function r_local_class() {
  class L {
    m(x) {
      if (x) {
        return 1;
      }
    }
  }
  return L;
}

function r_object_method(x) {
  const o = {
    m() {
      if (x) {
        return 1;
      }
    },
  };
  return o;
}

function r_arrow_in_declaration(a) {
  const f = () => {
    if (a) {
      return 1;
    }
  };
  return f;
}

function r_bool_in_declaration(a, b) {
  let x = a && b;
  return x;
}

function r_labeled_continue(xs) {
  outer: for (const x of xs) {
    if (x) {
      continue outer;
    }
  }
}

function r_labeled_break(xs) {
  outer: for (const x of xs) {
    if (x) {
      break outer;
    }
  }
}

function r_labeled_block_break(a) {
  block: {
    if (a) {
      break block;
    }
  }
}

const r_obj = {
  m(a) {
    if (a) {
      return 1;
    }
  },
  a: (b) => {
    if (b) {
      return 1;
    }
  },
};

class RField {
  h = (a) => {
    if (a) {
      return 1;
    }
  };
}

const RExpr = class RInner {
  m(a) {
    if (a) {
      return 1;
    }
  }
};

class ROuter {
  static Inner = class {
    n(a) {
      if (a) {
        return 1;
      }
    }
  };
}

if (globalThis) {
  function r_in_module_if(a) {
    if (a) {
      return 1;
    }
  }
}

try {
  function r_in_module_try(a) {
    if (a) {
      return 1;
    }
  }
} catch (e) {}

const RAsg = {};
RAsg.m = (a) => {
  if (a) {
    return 1;
  }
};

const RAsg2 = {};
RAsg2.p = {
  n(a) {
    if (a) {
      return 1;
    }
  },
};

if (!globalThis) {
} else {
  function r_in_module_else(a) {
    if (a) {
      return 1;
    }
  }
}

for (let i = 0; i < 1; i++) {
  function r_in_module_for(a) {
    if (a) {
      return 1;
    }
  }
}

for (const k in globalThis) {
  function r_in_module_for_in(a) {
    if (a) {
      return 1;
    }
  }
}

while (!globalThis) {
  function r_in_module_while(a) {
    if (a) {
      return 1;
    }
  }
}

do {
  function r_in_module_do(a) {
    if (a) {
      return 1;
    }
  }
} while (!globalThis);

switch (globalThis) {
  case null:
    function r_in_module_case(a) {
      if (a) {
        return 1;
      }
    }
}

try {
} catch (e) {
  function r_in_module_catch(a) {
    if (a) {
      return 1;
    }
  }
}

try {
} finally {
  function r_in_module_finally(a) {
    if (a) {
      return 1;
    }
  }
}

r_label: {
  function r_in_module_label(a) {
    if (a) {
      return 1;
    }
  }
}

function* r_gen(a) {
  if (a) {
    yield 1;
  }
}
