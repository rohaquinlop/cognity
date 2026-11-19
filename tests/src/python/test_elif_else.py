def p01_elif(a, b):
    if a:
        pass
    elif b:
        pass


def p02_if_else(a):
    if a:
        pass
    else:
        pass


def p03_nest_in_elif(a, b, c):
    if a:
        pass
    elif b:
        if c:
            pass


def p04_nest_in_else(a, c):
    if a:
        pass
    else:
        if c:
            pass


def p47_elif_bool(a, b, c):
    if a:
        pass
    elif b and c:
        pass


def p61_elif_chain_else(a, b):
    if a:
        pass
    elif b:
        pass
    else:
        pass


def p67_else_nested_in_for(xs, a):
    for x in xs:
        if a:
            pass
        else:
            pass
