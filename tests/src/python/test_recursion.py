def p27_recursion_twice(n):
    p27_recursion_twice(n - 1)
    return p27_recursion_twice(n - 1)


def p28_indirect_a(n):
    return p28_indirect_b(n)


def p28_indirect_b(n):
    return p28_indirect_a(n)


class P29:
    def m(self):
        return self.m()


def q18_nested_def_recursive_inner(n):
    def g(m):
        return g(m)
    return g(n)


def outer_and_inner(n):
    def inner(k):
        if k:
            return inner(k - 1)
        return 0
    return outer_and_inner(inner(n))


@deco
def decorated_recursive(n):
    return decorated_recursive(n - 1)
