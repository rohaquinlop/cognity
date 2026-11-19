function fact(n) {
  if (n) {
    return n * fact(n - 1);
  }
  return 1;
}

function ping(n) {
  return pong(n);
}

function pong(n) {
  return ping(n);
}

function shadowParam(shadowParam) {
  return shadowParam();
}

class Walker {
  walk(n) {
    return this.walk(n - 1);
  }

  visit(n) {
    const f = function () {
      return this.visit(n);
    };
    return f;
  }

  arrow(n) {
    const f = () => this.arrow(n);
    return f;
  }

  static s(n) {
    return this.s(n);
  }

  mix(n) {
    return this.s(n);
  }
}

function outer(n) {
  function inner(k) {
    return inner(k);
  }
  return inner(n);
}

class Literal {
  m() {
    return {
      m() {
        return this.m();
      },
    };
  }
}
