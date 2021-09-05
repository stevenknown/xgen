int foo()
{
    int c, e, *b;
    int * v;
    int ** y = &b; //y->b

    int * z = &c; //y->b, z->c

    int ** x = y; //y->b, z->c, x->b

    *x = z; //y->b, z->c, x->b, b->c

    v = &e; //y->b, z->c, x->b, b->c, v->e

    *y = v; //y->b, z->c, x->b, b->e, v->e

    return *y;
}
