int a[100];
int b[100];
int c[100];
int d[100];
void foo(int p)
{
    int i = 1;
    int x = 5;
    for (i=1;i<80;i++, x++) {
        a[i]=b[i]+c[i];
        a[x]=b[i]>>3;
    }
}
