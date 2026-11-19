def p32_return_inner_call(b):
    def inner():
        if b:
            pass
    return inner()


def p53_async_for(xs, a):
    async def g():
        async for x in xs:
            if a:
                pass
    return g()


def p33_decorator_docstring(f):
    """Doc."""
    def inner():
        if f:
            pass
    return inner


def r15_root_decorator_return_expr(f, a):
    def inner():
        pass
    return wraps(f if a else None)(inner)


def p34_decorator_wraps(f):
    def inner():
        if f:
            pass
    return wraps(f)(inner)


def r06_nested_decorator_no_flatten(a):
    def deco(fn):
        def inner():
            if a:
                pass
        return inner
    return deco(1)
