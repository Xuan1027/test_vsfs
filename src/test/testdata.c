#include "vsfs.h"

int main(int argc, char** argv) {
  FILE* fp = fopen("./src/test/testdata.txt", "w");

  for (int i = 0; i < 555555; i++) fprintf(fp, "%c", 'a');

  fclose(fp);

  return 0;
}