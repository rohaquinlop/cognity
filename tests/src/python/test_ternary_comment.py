def ternary_with_comment(a, b, c):
    return (
        a  # body
        if b
        else c
    )


def annotated(a, b):
    x: int if a else str = b or a
    return x
