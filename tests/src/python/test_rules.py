def r_if(a):
    if a:
        return 1


def r_else_if(a, b):
    if a:
        return 1
    elif b:
        return 2


def r_else(a):
    if a:
        return 1
    else:
        return 2


def r_nest_in_else(a, b):
    if a:
        return 1
    else:
        if b:
            return 2


def r_for(xs):
    for x in xs:
        if x:
            return x


def r_while(a):
    while a:
        a -= 1


def r_switch(a):
    match a:
        case 1:
            if a:
                return 1
        case _:
            return 0


def r_try_catch_finally(a):
    try:
        if a:
            return 1
    except ValueError as e:
        if e:
            return 2
    finally:
        if a:
            a = 0


def r_ternary(a):
    return 1 if a else 2


def r_ternary_nested(a, b):
    return (1 if b else 2) if a else 3


def r_bool_seq(a, b, c, d):
    return a and b and c or d


def r_not(a, b):
    return not (a and b)


def r_lambda_nesting(xs):
    return list(map(lambda x: 1 if x else 0, xs))


def r_nested_function(a):
    def inner():
        if a:
            return 1
    return inner()


def r_recursion(n):
    return r_recursion(n - 1)


def r_break_continue(xs):
    for x in xs:
        if x:
            continue
        break


def r_with(a, b):
    with a:
        if b:
            return 1


def r_loop_else(xs):
    for x in xs:
        pass
    else:
        return 0


class K:
    def m(self, a):
        if a:
            return 1
