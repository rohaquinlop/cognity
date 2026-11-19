def p12_case_guard(v, a, b):
    match v:
        case 1 if a and b:
            pass


def p66_if_in_case_guard_nested(a, v, b, c):
    if a:
        match v:
            case 1 if b or c:
                pass
