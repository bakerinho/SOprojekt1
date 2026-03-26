#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Użycie: %s <katalog_zrodlowy> <katalog_docelowy>\n", argv[0]);
    return 1;
  }

  const char *srcPath = argv[1];
  const char *dstPath = argv[2];
  struct stat statbuf;

  if (stat(srcPath, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
    printf(
        "Blad: Sciezka zrodlowa '%s' nie istnieje lub nie jest katalogiem.\n",
        srcPath);
    return 1;
  }

  if (stat(dstPath, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
    printf(
        "Blad: Sciezka docelowa '%s' nie istnieje lub nie jest katalogiem.\n",
        dstPath);
    return 1;
  }

  // Usuniecie nadmiaru plikow z dst
  DIR *dstDir;
  struct dirent *pDirent;

  dstDir = opendir(dstPath);

  if (dstDir == NULL) {
    printf("Nie mozna otworzyc katalogu '%s'", dstPath);
    return 1;
  }

  char filePathSrc[1024];
  strcpy(filePathSrc, srcPath); 
  strcat(filePathSrc, pDirent->d_name);
  printf("%s\n", filePathSrc);

  while ((pDirent = readdir(dstDir)) != NULL) {
    if ((strcmp(pDirent->d_name, ".") == 0) || strcmp(pDirent->d_name, "..") == 0) {
      continue;
    }
    printf("%s\n", pDirent->d_name);



  }

  closedir(dstDir);

  return 0;
}
