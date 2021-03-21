struct S { int dummy; short e, f; char g[13]; int h;} x,y;
int main() {
    struct S a; //a is aligned in target default alignment.
#pragma align(8)
    struct S b; //b is aligned in 8 bytes.
#pragma align(16)
    struct S c; //c is aligned in 16 bytes.

    if (&a % 4 != 0) { 
        return 1;
    }
    if (&b % 8 != 0) { 
        return 2;
    }
    if (&c % 16 != 0) { 
        return 3;
    }
    return 0;
} 
