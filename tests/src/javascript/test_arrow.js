function arrowBlock(xs) {
  return xs.map((x) => {
    if (x) {
      return 1;
    }
    return 0;
  });
}

function arrowExprBody(xs) {
  return xs.filter((x) => (x ? 1 : 0));
}

function arrowDefault(xs) {
  return xs.map((x, y = x && 1) => y);
}

function functionExpr(a) {
  const g = function () {
    while (a) {
      a--;
    }
  };
  return g;
}
