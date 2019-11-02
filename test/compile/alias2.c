int main()
{
    //verify flow insensitive point to.
    char d[100];
    int a,b,*p, i, c;
    for (;;) {
        if (i) {
            p = &a;
            i += d[c];
            *p = 2;
        } else if (i > 12) {
            p = &b;
            *p = 3 - d[i] + a;
        } else {
            p = &d[i];
        }
        *p = 10;
        *p = 20;
    }
}
