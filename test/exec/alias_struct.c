int printf(char const*,...);
struct S { int dummy; short e, f; } x,y;
struct S* p = &x;

int foobar() {
  x.f=42;
  *p=y;  //**** struct copy
  return x.f;
}

int main() {
    y.f = 1;
    p = &x;
    foobar();
    if (1 == x.f) {
        printf("\nsuccess\n");
        return 0;
    }
    return 1;
} 
 
