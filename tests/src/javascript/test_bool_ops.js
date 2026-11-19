function strictNe(a, b) {
  if (a !== b) {
    return 1;
  }
  return 0;
}

function notAlone(a) {
  return !a;
}

function notOperand(a, b) {
  return !(a && b);
}

function mixedRuns(a, b, c, d) {
  return a && b || c && d;
}

function parenRun(a, b, c) {
  return a && (b && c);
}

function andNotAnd(a, b, c) {
  return a && !b && c;
}

function nullish(a, b) {
  return a ?? b;
}

function doWhileBool(a, b) {
  do {
    a--;
  } while (a && b);
}
