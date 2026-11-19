def p30_nested_def_in_if(a, b):
    if a:
        def g():
            if b:
                pass
        g()


def r05_ternary_in_nested_def(a, b, c):
    def g():
        x = a if b else c
    g()
