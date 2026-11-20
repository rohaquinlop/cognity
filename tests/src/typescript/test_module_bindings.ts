export const a = (x: number): number => {
  if (x > 0) {
    return 1;
  }
  return 0;
};

const b: (x: boolean) => boolean = (x) => x && y || z;

let c = (x: number, y: number = p || q) => x;

export class K {
  m(x: number) {
    if (x) {}
  }
}
