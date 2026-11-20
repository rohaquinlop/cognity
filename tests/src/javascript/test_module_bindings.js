const a = (x) => {
  if (x) {
    return 1;
  }
  return 0;
};

let b = function (x) {
  for (;;) {
    if (x) {}
  }
};

var c = function* (x) {
  while (x && y) {}
};

const d = (x, y) => x && y || z;

const e = (x = p && q) => x ? 1 : 0;

export const f = ((x) => {
  if (x) {} else {}
});

export function g(x) {
  if (x) {}
}

const h = 1, k = (x) => x ? 1 : 0;

const o = () => {
  const i = () => {
    if (1) {}
  };
};

const { m } = obj;
const n = [() => { if (1) {} }];
