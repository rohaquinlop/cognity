def p35_lambda_ternary():
    g = lambda x: 1 if x else 2


def p56_lambda_in_if(a, b, c, d):
    if a:
        g = lambda: b if c else d


def q17_assign_ternary_in_lambda_arg(a, b, c):
    x = foo(lambda: a if b else c)
