struct T
{
  unsigned int b, c, *d;
  unsigned char e;
};
struct S
{
  unsigned int a;
  int m[10];
  struct T f;
};
struct U
{
  struct S g;
  int n[10];
  struct S h;
};

int main()
{
  struct S s = { 1, {2, 3}, {4, 5} };
  if (s.a !=  1) { return 1; }   
  if (s.m[0] != 2) { return 1; }   
  if (s.m[1] != 3) { return 1; }   
  if (s.f.b != 4) { return 1; }   
  if (s.f.c != 5) { return 1; }   
  
  struct U u = { 
    {7, {8, 9} },
    { 20,21,22, },
    {10, {11,12,(char*)13,14} }
  };
  if (u.n[0] != 20) { return 3; }
  if (u.n[1] != 21) { return 3; }
  if (u.n[2] != 22) { return 3; }
  if (u.g.a != 7) { return 3; }
  if (u.g.f.b != 8) { return 3; }
  if (u.g.f.c != 9) { return 3; }
  if (u.h.a != 10) { return 3; }
  if (u.h.f.b != 11) { return 3; }
  if (u.h.f.c != 12) { return 3; }
  if (u.h.f.d != (char*)13) { return 3; }
  if (u.h.f.e != 14) { return 3; }

  return 0;
}
