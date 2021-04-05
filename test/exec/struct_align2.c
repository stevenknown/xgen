//Cause IR2OR assert.
struct A {
   int a;
   short mpr[1024][1024];
} ga;

struct B {
   short mpr[1024][1024];
} gb;
int main()
{
    struct A * a;
    a = &ga;

    if (sizeof(struct B) != sizeof(short)*1024*1024) { return 1; }
    return 0;
}
