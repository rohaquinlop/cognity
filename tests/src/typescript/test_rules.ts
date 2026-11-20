function r_if(a: number): unknown {
  if (a) {
    return 1;
  }
}

function r_else_if(a: number, b: number): unknown {
  if (a) {
    return 1;
  } else if (b) {
    return 2;
  }
}

function r_else(a: number): unknown {
  if (a) {
    return 1;
  } else {
    return 2;
  }
}

function r_nest_in_else(a: number, b: number): unknown {
  if (a) {
    return 1;
  } else {
    if (b) {
      return 2;
    }
  }
}

function r_for(xs: number[]): unknown {
  for (const x of xs) {
    if (x) {
      return x;
    }
  }
}

function r_while(a: number): unknown {
  while (a) {
    a--;
  }
}

function r_do_while(a: number): unknown {
  do {
    a--;
  } while (a);
}

function r_switch(a: number): unknown {
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

function r_try_catch_finally(a: number): unknown {
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

function r_ternary(a: number): unknown {
  return a ? 1 : 2;
}

function r_ternary_nested(a: number, b: number): unknown {
  return a ? (b ? 1 : 2) : 3;
}

function r_bool_seq(a: number, b: number, c: number, d: number): unknown {
  return a && b && c || d;
}

function r_not(a: number, b: number): unknown {
  return !(a && b);
}

function r_arrow_nesting(xs: number[]): unknown {
  return xs.map((x) => {
    if (x) {
      return 1;
    }
  });
}

function r_nested_function(a: number): unknown {
  function inner(): unknown {
    if (a) {
      return 1;
    }
  }
  return inner();
}

function r_recursion(n: number): unknown {
  return r_recursion(n - 1);
}

function r_break_continue(xs: number[]): unknown {
  for (const x of xs) {
    if (x) {
      continue;
    }
    break;
  }
}

class K {
  m(a: number): unknown {
    if (a) {
      return 1;
    }
  }
}

function r_unbraced_nested_if(a: number, c: number): unknown {
  if (a) if (c) return 1;
  return 0;
}

function r_unbraced_loop_body(x: number): unknown {
  while (x) if (x > 1) x--;
}

function r_local_class(): unknown {
  class L {
    m(x: number): unknown {
      if (x) {
        return 1;
      }
    }
  }
  return L;
}

function r_object_method(x: number): unknown {
  const o = {
    m(): unknown {
      if (x) {
        return 1;
      }
    },
  };
  return o;
}

function r_arrow_in_declaration(a: number): unknown {
  const f = (): unknown => {
    if (a) {
      return 1;
    }
  };
  return f;
}

function r_bool_in_declaration(a: number, b: number): unknown {
  let x = a && b;
  return x;
}

function r_labeled_continue(xs: number[]): unknown {
  outer: for (const x of xs) {
    if (x) {
      continue outer;
    }
  }
}

function r_labeled_break(xs: number[]): unknown {
  outer: for (const x of xs) {
    if (x) {
      break outer;
    }
  }
}

function r_labeled_block_break(a: number): unknown {
  block: {
    if (a) {
      break block;
    }
  }
}

const r_obj = {
  m(a: number): unknown {
    if (a) {
      return 1;
    }
  },
  a: (b: number): unknown => {
    if (b) {
      return 1;
    }
  },
};

class RField {
  h = (a: number): unknown => {
    if (a) {
      return 1;
    }
  };
}

const RExpr = class RInner {
  m(a: number): unknown {
    if (a) {
      return 1;
    }
  }
};

class ROuter {
  static Inner = class {
    n(a: number): unknown {
      if (a) {
        return 1;
      }
    }
  };
}

if (globalThis) {
  function r_in_module_if(a: number): unknown {
    if (a) {
      return 1;
    }
  }
}

try {
  function r_in_module_try(a: number): unknown {
    if (a) {
      return 1;
    }
  }
} catch (e) {}

const RAsg: any = {};
RAsg.m = (a: number): unknown => {
  if (a) {
    return 1;
  }
};

const RAsg2: any = {};
RAsg2.p = {
  n(a: number): unknown {
    if (a) {
      return 1;
    }
  },
};

abstract class RAbs {
  abstract k(): void;
  m(a: number): unknown {
    if (a) {
      return 1;
    }
  }
}

abstract class RAbsRec {
  m(n: number): unknown {
    if (n) {
      return this.m(n - 1);
    }
  }
}

namespace RNs {
  export function f(a: number): unknown {
    if (a) {
      return 1;
    }
  }
}

namespace RNs2 {
  class K {
    m(a: number): unknown {
      if (a) {
        return 1;
      }
    }
  }
}

namespace RA.RB {
  export function g(a: number): unknown {
    if (a) {
      return 1;
    }
  }
}
