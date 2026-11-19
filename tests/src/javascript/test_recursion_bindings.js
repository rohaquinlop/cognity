function a1(n) {
  return a1(n - 1);
}
a1 = wrap(a1);

function b1(xs) {
  for (const b1 of xs) {
    b1();
  }
}

function c1(o) {
  for (const c1 in o) {
    o[c1]();
    c1();
  }
}

function d1() {
  try {
    g();
  } catch (d1) {
    d1();
  }
}

function e1(n) {
  return e1(n - 1);
}
[e1] = xs;

function f1(n) {
  return f1(n - 1);
}
({ f1 } = o);

function g1(xs) {
  for (g1 of xs) {
    g1();
  }
}

h1 = 1;
function h1(n) {
  return h1(n - 1);
}

function i1(n) {
  return i1(n - 1);
}
o.i1 = 1;

function j1(n) {
  function k() {
    let j1;
    j1 = 2;
  }
  return j1(n - 1);
}

function k1(n) {
  return k1(n - 1);
}
k1 ||= g;

function l1() {
  try {
    g();
  } catch ({ l1 }) {
    l1();
  }
}

function m1(n) {
  return m1(n - 1);
}
[o.m1] = xs;
