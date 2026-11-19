def p08_finally_not_nested(a):
    try:
        pass
    finally:
        if a:
            pass


def p10_with_not_nested(x, a):
    with x:
        if a:
            pass
