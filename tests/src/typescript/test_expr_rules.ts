function strictNe(a: number, b: number): number {
  return a !== b ? 1 : 0;
}

function nested(a: boolean, b: boolean): number {
  return a ? (b ? 1 : 2) : 3;
}

function arrow(xs: number[]): number[] {
  return xs.map((x: number, y: boolean = x > 0 && true) => (y ? x : 0));
}
