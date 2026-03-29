#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 1024

void delete_recursive(const char *path) {
  printf("Aktualna sciezka: %s\n", path);
  DIR *dir;
  struct dirent *entry;

  dir = opendir(path);

  if (dir == NULL) {
    return;
  }

  struct stat statbuf;
  char file_path[MAX_PATH_LENGTH];

  while ((entry = readdir(dir)) != NULL) {
    if ((strcmp(entry->d_name, ".") == 0) || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    snprintf(file_path, sizeof(file_path), "%s", path);
    if (lstat(file_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
      printf("Znaleziono katalog: %s", entry->d_name);
      return delete_recursive(file_path);
    }
  }
  closedir(dir);
}

void synchronize(const char *src_path, const char *dst_path) {

  // Usuniecie nadmiaru plikow z dst
  DIR *dir;
  struct dirent *entry;

  dir = opendir(dst_path);

  if (dir == NULL) {
    printf("Nie mozna otworzyc katalogu '%s'", dst_path);
    return;
  }

  struct stat st_src;
  struct stat st_dst;

  char file_path_src[MAX_PATH_LENGTH];
  char file_path_dst[MAX_PATH_LENGTH];

  while ((entry = readdir(dir)) != NULL) {
    if ((strcmp(entry->d_name, ".") == 0) || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    // printf("%s\n", pDirent->d_name);
    snprintf(file_path_src, sizeof(file_path_src), "%s/%s", src_path,
             entry->d_name);
    snprintf(file_path_dst, sizeof(file_path_dst), "%s/%s", dst_path,
             entry->d_name);
    // printf("%s\n", filePathSrc);
    // printf("%s\n", filePathDst);

    if (lstat(file_path_dst, &st_dst) == 0 && S_ISREG(st_dst.st_mode)) {

      if (lstat(file_path_src, &st_src) != 0) {
        printf("Nie ma: %s\n", entry->d_name);
        // TODO usunac ten nadmiar plikow
      }
      // else {
      //   printf("Jest: %s\n", pDirent->d_name);
      // }
    }
  }

  closedir(dir);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Użycie: %s <katalog_zrodlowy> <katalog_docelowy>\n", argv[0]);
    return 1;
  }

  const char *src_path = argv[1];
  const char *dst_path = argv[2];
  struct stat statbuf;

  if (lstat(src_path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
    printf(
        "Blad: Sciezka zrodlowa '%s' nie istnieje lub nie jest katalogiem.\n",
        src_path);
    return 1;
  }

  if (lstat(dst_path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
    printf(
        "Blad: Sciezka docelowa '%s' nie istnieje lub nie jest katalogiem.\n",
        dst_path);
    return 1;
  }

  synchronize(src_path, dst_path);

  return 0;
}
