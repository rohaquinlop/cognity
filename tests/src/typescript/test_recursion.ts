function fib(n: number): number {
  return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

class Tree {
  depth(n: number = 0): number {
    return this.depth(n + 1);
  }
}
