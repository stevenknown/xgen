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

int main()
{
  union T t = { 1, 2, (void*)0x1000, 3};
  if (t.b != 0x1003) { return 1; }
  if (t.c != 0x1003) { return 2; }
  if (t.d != (void*)0x1003) { return 3; }
  if (t.e != 3) { return 4; }
  
  union S s = { 4, {5, 6}, };
  if (s.a != 6) { return 2; }   
  if (s.f.b != 6) { return 2; }   
  if (s.f.c != 6) { return 2; }   
  
  union U u = { 
    {7, {8, 9} },
    {10, {11,12,(char*)13,14} }
  };
  if (u.g.a != 14) { return 3; }
  if (u.g.f.b != 14) { return 3; }
  if (u.g.f.c != 14) { return 3; }
  if (u.h.a != 14) { return 3; }
  if (u.h.f.b != 14) { return 3; }
  if (u.h.f.c != 14) { return 3; }
  if (u.h.f.d != (char*)14) { return 3; }
  if (u.h.f.e != 14) { return 3; }

  return 0;
}

