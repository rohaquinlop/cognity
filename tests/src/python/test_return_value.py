def q01_return_bool(a, b):
    return a or b


def q03_return_ternary(a, b, c):
    return a if b else c


def p51_not_group(a, b, c):
    return not (a and (b or c))


def p52_and_not_group(a, b, c, d):
    return a and not (b or c) and (c or d)


def p59_comp_ternary_in_if(a, b, ys):
    if a:
        return [1 if b else 2 for x in ys]


def p21_comprehension_bool(ys, z):
    return [x for x in ys if x and z]
