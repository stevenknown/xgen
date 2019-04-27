void putchar(char);
void printf(char const*,...);
int main() {
    float f;
    f = 1.5f;
    int i;
    for (i=0;i<9;i++){
      char d;
      char c;
      c = ".:-=+*#%@"[i];
      d = f <= 0.0f ? c : ' ';
      putchar(d);
    }
    printf("\nfinish\n");
}


