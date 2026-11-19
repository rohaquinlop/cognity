def q13_case_body_only(v, a):
    match v:
        case 1:
            if a:
                pass


def q14_match_only(v):
    match v:
        case 1:
            pass


def p11_match_case_nest(v, a):
    match v:
        case 1:
            if a:
                pass
        case _:
            pass
