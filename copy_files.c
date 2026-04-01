#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[])
{
  if (argc != 5) {
    printf("Za malo argumentow");
    return 1;
  }

  const char *srcFile = argv[1];
  const char *dstFile = argv[2];

  char *endPtr;
  long buffSize = strtol(argv[3], &endPtr, 10);
  char *buff = malloc(buffSize);

  if (!buff) {
    printf("allocate error\n");
    return 1;
  }

  int fSrc = open(srcFile, O_RDONLY);
  if(fSrc == -1){
    return errno; 
  }

  int fOut = open(dstFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if(fOut == -1){
     close(fSrc);
     return errno;
  }

  int bytes;
  while ((bytes = read(fSrc, buff, buffSize)) > 0) {

   if(bytes != write(fOut, buff, bytes)) {
      perror("write");
      close(fSrc);
      close(fOut);
      free(buff);
      return 1;
   }
   if (bytes == -1) {
      perror("read");
      close(fSrc);
      close(fOut);
      free(buff);
      return 1;
    }
  }

  close(fSrc);
  close(fOut);
  free(buff);

  return 0;
}
