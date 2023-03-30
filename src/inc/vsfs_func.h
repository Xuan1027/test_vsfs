#ifndef VSFS_FUNC_H
#define VSFS_FUNC_H


#include "vsfs_stdinc.h"
#include <locale.h>
/**
 * this is a func printting the progress bar
 * @param progress is the work you already done
 * @param total is the total work need to be done
 */
void progress_bar(size_t progress, size_t total) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  int winsize = w.ws_col - 20, loading_size;

  loading_size = progress * winsize / total;
  printf("\rprogressing..[");
  for (int i = 0; i < winsize; i++) {
    if (i < loading_size)
      printf("#");
    else
      printf(".");
  }
  printf("] %d%%", loading_size * 100 / winsize);
  fflush(stdout);

  if ((loading_size / winsize) == 1)
    printf("\n");
}

#endif