struct defs {
  int ubits;
};

int main(struct defs * pd0)
{
  char *ps, *pt;
  int rc, t, lrc, k, j;
  while (*pt = *ps) { //load *ps is loop invariant.
    //never quitted loop
  }
  for (k = 0; k < pd0->ubits; k++) //pd0->ubits is loop invariant.
    for (j = 0; j < pd0->ubits; j++) //pd0->ubits is loop invariant.
      lrc = 1;
  return rc;
}
