function a1(n: number): number {
  return a1(n - 1);
}
a1 = wrap(a1);

function b1(xs: Array<() => void>): void {
  for (const b1 of xs) {
    b1();
  }
}

function c1(): void {
  try {
    g();
  } catch (c1: any) {
    c1();
  }
}
