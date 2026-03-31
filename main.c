#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 1024

void print_help(const char *name);

void delete_recursive(const char *path) {
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

    snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

    if (lstat(file_path, &statbuf) == 0) {
      if (S_ISDIR(statbuf.st_mode)) {
        delete_recursive(file_path);
      } else {
        unlink(file_path);
      }
    }
  }
  closedir(dir);
  // usuniecie folderu z ktorego sie zaczelo
  rmdir(path);
}

void synchronize(const char *src_path, const char *dst_path, int recursion) {

  // Usuniecie nadmiaru plikow z dst
  DIR *dir;
  struct dirent *entry;

  dir = opendir(dst_path);

  if (dir == NULL) {
    printf("Nie mozna otworzyc katalogu '%s'\n", dst_path);
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
    snprintf(file_path_src, sizeof(file_path_src), "%s/%s", src_path,
             entry->d_name);
    snprintf(file_path_dst, sizeof(file_path_dst), "%s/%s", dst_path,
             entry->d_name);

    if (lstat(file_path_dst, &st_dst) == 0 && S_ISREG(st_dst.st_mode)) {

      if (lstat(file_path_src, &st_src) != 0 || !S_ISREG(st_src.st_mode)) {
        unlink(file_path_dst);
      }
    }

    if (recursion) {
      if (lstat(file_path_dst, &st_dst) == 0 && S_ISDIR(st_dst.st_mode)) {
        if (lstat(file_path_src, &st_src) != 0 || !S_ISDIR(st_src.st_mode)) {
          delete_recursive(file_path_dst);
        }
      }
    }
  }
  closedir(dir);

  // synchronizacja plikow z src do dsc
}

int main(int argc, char *argv[]) {
  const char *src_path = NULL;
  const char *dst_path = NULL;

  int recursion = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      print_help(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-R") == 0) {
      recursion = 1;
    } else if (src_path == NULL) {
      src_path = argv[i];
    } else if (dst_path == NULL) {
      dst_path = argv[i];
    } else {
      printf("Podano zbyt duzo argumentow\n");
      return 1;
    }
  }

  if (src_path == NULL || dst_path == NULL) {
    printf(
        "Wymagane sa dwa argumenty: <katalog zrodlowy> <katalog docelowy>\n");
    print_help(argv[0]);
    return 1;
  }

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

  synchronize(src_path, dst_path, recursion);

  return 0;
}

void print_help(const char *name) {
  printf("Demon synchronizujący dwa podkatalogi\n");
  printf("------------------------------\n");
  printf("Uzycie: %s [-R] [-h] <zrodlo> <cel>\n", name);
  printf("Parametry:\n");
  printf("  <zrodlo>    Katalog, z ktorego kopiujemy dane\n");
  printf("  <cel>       Katalog, ktory uaktualniamy i sprzatamy\n");
  printf("Opcje:\n");
  printf("  -R          Synchronizacja rekurencyjna (wchodzi w podkatalogi)\n");
  printf("  -h          Wyswietla pomoc\n");
}
