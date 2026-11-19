def q04_assign_mixed_ltr(a, b, c, d):
    x = a or b and c or d


def q05_assign_mixed_paren(a, b, c, d):
    x = (a or (b and c)) and d


def q06_assign_two_groups(a, b, c, d):
    x = (a or b) and (c and d)


def q07_assign_three(a, b, c, d):
    x = (a or b) and c and (c or d)


def q08_assign_not_group(a, b, c):
    x = not (a and (b or c))


def q09_assign_and_not_group(a, b, c, d):
    x = a and not (b or c) and (c or d)


def q10_assign_not_alone(a):
    x = not a


def q16_while_not(a):
    while not a:
        pass


def p14_mixed_bool_ltr(a, b, c, d):
    return a or b and c or d


def p15_mixed_bool_paren(a, b, c, d):
    return (a or (b and c)) and d


def p16_not_alone(a):
    if not a:
        pass
