int g;
int n;
int z;
int main(int b)
{
  goto LOOP_HEAD;

 LOOP_START: 
   n = 10;
LOOP_HEAD:
  //Should insert guard BB to control preheader.
  g = b;
  if (b > 0) { goto LOOP_START; }
  return g;
}
