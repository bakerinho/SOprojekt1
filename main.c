#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

int isDir(const char *path) {
  struct stat statbuf;
  // 1 - katalog
  // 0 - poprawna sciezka nieprowadzaca do katalogu
  // -1 - zla sciezka
  if (stat(path, &statbuf) != 0) {
    return -1;
  }

  return S_ISDIR(statbuf.st_mode) ? 1 : 0;
}

int validateDir(const char *path, const char *type) {
  int status = isDir(path);

  if (status == -1) {
    printf("Nieistniejaca sciezka %s (%s)\n", type, path);
    return 1;
  }

  if (status == 0) {
    printf("Sciezka %s nie jest katalogiem (%s)\n", type, path);
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Użycie: %s <katalog_zrodlowy> <katalog_docelowy>\n", argv[0]);
    return 1;
  }

  const char *srcPath = argv[1];
  const char *dstPath = argv[2];

  if (validateDir(srcPath, "Zrodlo") != 0) {
    return 1;
  }

  if (validateDir(dstPath, "Docelowa") != 0) {
    return 1;
  }

  return 0;
}
