function forOf(xs, a, b) {
  for (const x of a || b) {
    if (x) {
      return x;
    }
  }
}

function forClassic(n, a) {
  for (let i = 0; i < n && a; i++) {
    a--;
  }
}

function switchValue(a, b) {
  switch (a && b) {
    case true:
      break;
  }
}
