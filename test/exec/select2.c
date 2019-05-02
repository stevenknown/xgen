void putchar(char);
void printf(char const*,...);
int main() {
    int f;
    f = 0;
    int i;
    char d;
    char c;
    for (i=0;i<9;i++){
      c = ".:-=+*#%@"[i];
      d = f <= 0 ? c : '$';
      putchar(d);
    }
    printf("\nfinish\n");
}
