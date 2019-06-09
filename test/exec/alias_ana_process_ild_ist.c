void abort();
void exit(int);
void printf(char const*,...);
void process_ild_ist_struct()
{
    struct S {
      char ch[200];
      int a,b,c;
    } a[100];
    int * p;
    struct S * q;
    p = &a;
    q = &a[13];
    int i;
    i = 100;
    while (i) {
      *p = (*q).b;
      i--;
    }
}

void process_ild_ist()
{
    int * p;
    long long * q;
    char a[100];
    p = &a;
    q = &a + 13;
    int i;
    i = 100;
    a[0] = 27;
    a[13] = 27;
    while (i) {
      *p = *q;
      i--;
    }
    if (*p != *q || *q != 27) {
        abort();
    }
}

int main()
{
    process_ild_ist();
    printf("\nsuccess\n");
    return 0;
}
