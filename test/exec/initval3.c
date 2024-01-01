union T
{
  unsigned int b, c, *d;
  unsigned char e;
};
union S
{
  unsigned int a;
  union T f;
};
union U
{
  union S g;
  union S h;
};
union Q
{
  unsigned char n;
  union S2 {
    unsigned short w;
    unsigned long long m;
  } s;
};
void printf(char const*,...);
int main()
{
  union S s = { 4, 5, 6, };
  if (s.a != 4) { return 2; }
  if (s.f.b != 4) { return 2; }
  if (s.f.c != 4) { return 2; }

  union U u = {
    {7, 8, 9, },
    10, 11,12,(char*)13,14,
  };
  if (u.g.a != 7) { return 3; }
  if (u.g.f.b != 7) { return 3; }
  if (u.g.f.c != 7) { return 3; }
  if (u.h.a != 7) { return 3; }
  if (u.h.f.b != 7) { return 3; }
  if (u.h.f.c != 7) { return 3; }
  if (u.h.f.d != (char*)0x7) { return 3; }
  if (u.h.f.e != 7) { return 3; }

  union Q q = { 0x00000000000000cc, 0xFFFFffee, };
  if (((unsigned char)q.s.m) != 0xcc) { return 4; }
  if (q.n != 0xcc) { return 5; }
  return 0;
}
