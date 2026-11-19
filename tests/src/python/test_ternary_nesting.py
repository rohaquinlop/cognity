def r03_ternary_in_try_body(a, b, c):
    try:
        x = a if b else c
    except ValueError:
        pass


def r04_ternary_in_with_body(w, a, b, c):
    with w:
        x = a if b else c
