int foo(short b, short a)
{
    //Promote x[2], *q, and *(p+2) into same PREG.
    int x[10];
    int i;
    int * p;
    int * q;
    p = &x[0];
    q = &x[2];
    for (i = 0; i < 10; i++) {
        x[2] = *q + 10;
        *p = 10;
        //*(p+2) exactly alias with x[2]
        *(p+2) = x[3] + x[2] + *q;
    }
}

