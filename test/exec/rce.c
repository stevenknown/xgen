int g;
int main()
{
    int * p = &g;
    for (;;) {
        *p = 20; //IR_CP will propagate and change *p to g.
                 //Have to CHECK the MDSSA DU
    }
    return 0;
}
