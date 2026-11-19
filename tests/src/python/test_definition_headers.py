def p36_class_in_function(a):
    class K:
        def m(self):
            if a:
                pass
    return K


def p39_nested_def_default(a, b):
    def g(x=a or b):
        pass
    g()
