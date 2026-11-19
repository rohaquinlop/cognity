namespace geo {
int depth(int n) {
  return depth(n - 1);
}
}  // namespace geo

class Node {
 public:
  int size(int n) {
    return this->size(n - 1);
  }
  int count(int n);
};

int Node::count(int n) {
  return count(n - 1);
}

int over(int a) { return a; }

int over(int a, int b) {
  return over(a);
}
