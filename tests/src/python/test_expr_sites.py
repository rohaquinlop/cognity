def p22_call_arg_bool(a, b):
    foo(a and b)


def p25_for_iter_bool(a, b):
    for x in a or b:
        pass


def p26_except_type(b):
    try:
        pass
    except (ValueError if b else TypeError):
        pass


def p13_match_subject(a, b, c):
    match (a if b else c):
        case _:
            pass


def p64_del_target(a, b, xs):
    del xs[a or b]


def p23_kwarg_ternary(a, b, c):
    foo(k=a if b else c)


def p62_dict_key_ternary(a, b, c):
    return {a if b else c: 1}


def p63_subscript_bool(a, b, xs):
    return xs[a or b]


def p24_binop_bool(a, b):
    return (a or b) + 1


async def p49_await_ternary(a, b, c):
    await foo(a if b else c)


def p50_fstring_ternary(a, b, c):
    return f"{a if b else c}"


def r12_for_target(xs, a, b):
    for ys[a or b] in xs:
        pass


def r13_aug_target(xs, a, b):
    xs[a or b] += 1
