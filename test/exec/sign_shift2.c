void printf(char const*,...);
int main()
{
    long long x=-1;
    long long result = (x >> (32));
    if (result != x) { return 1; }

    unsigned long long x2=-1;
    unsigned long x2_1 =-1;
    unsigned long long result2 = (x2 >> (32));
    if (result2 != x2_1) { return 2; }

    return 0;
}
