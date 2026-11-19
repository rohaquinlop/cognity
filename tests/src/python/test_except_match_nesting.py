def r01_except_nested_in_if(a):
    if a:
        try:
            pass
        except ValueError:
            pass


def r02_match_nested_in_if(a, v):
    if a:
        match v:
            case _:
                pass
