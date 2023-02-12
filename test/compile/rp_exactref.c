void printf(char const*,...);
volatile double gd[100];
void foo()
{
  float f;
  float *pf;
  pf = gd;
  for (int i = 0; i < 1; i++)
  {
    pf = gd;
    f = *(pf++);
  }
}
