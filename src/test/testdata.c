#include "vsfs.h"

int main(int argc, char** argv) {

  char *src = (char *)malloc(4096);

  FILE* fp = fopen("./src/test/testdata.txt", "r");

  int ret=0;
  ret = fscanf(fp, "%[^\n]", src);
  if(ret == -1)
    return -1;

  for (int i = 1; i <= 4096; i++) {
    fprintf(fp, "%d %s\n", i, src);
  }

  fclose(fp);

  free(src);

  return 0;
}