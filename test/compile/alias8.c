char * p;
int x,y;
int foo()
{
    int i;
    int * l;
    l = &i;
    //Now, the worst may-point-to set contains {MD2,i}
    //Actually, p pointed to {m,n}
    *p = 20;
    return x;
}
