#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 1024
#define BUFF_SIZE_OPEN 8192                 // 8KB bo tak
#define DEFAULT_SIZE_MMAP (8 * 1024 * 1024) // 8MB bo na jakims tescie widzialem

// TODO: zrobic fork()

void print_help(const char *name);

int delete_recursive(const char *path) {
  DIR *dir;
  struct dirent *entry;

  dir = opendir(path);

  if (dir == NULL) {
    return -1;
  }

  struct stat statbuf;
  char file_path[MAX_PATH_LENGTH];

  while ((entry = readdir(dir)) != NULL) {
    if ((strcmp(entry->d_name, ".") == 0) || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

    if (lstat(file_path, &statbuf) != 0) {
      printf("Nie znaleziono pliku docelowego '%s'", file_path);
      continue;
    }
    if (S_ISDIR(statbuf.st_mode)) {
      delete_recursive(file_path);
    } else {
      unlink(file_path);
    }
  }
  closedir(dir);
  // usuniecie folderu w ktorym usunieto wszystkie pliki
  rmdir(path);
  return 0;
}

int remove_file(const char *src_path, const char *dst_path, int recursion) {

  DIR *dir;
  struct dirent *entry;

  dir = opendir(dst_path);

  if (dir == NULL) {
    printf("Nie mozna otworzyc katalogu '%s'\n", dst_path);
    return -1;
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

    if (lstat(file_path_dst, &st_dst) != 0) {
      printf("Nie znaleziono pliku docelowego '%s'", file_path_dst);
      continue;
    }
    if (S_ISREG(st_dst.st_mode)) {
      if (lstat(file_path_src, &st_src) != 0 || !S_ISREG(st_src.st_mode)) {
        unlink(file_path_dst);
      }
    } else if (recursion && S_ISDIR(st_dst.st_mode)) {
      if (lstat(file_path_src, &st_src) != 0 || !S_ISDIR(st_src.st_mode)) {
        if (delete_recursive(file_path_dst) == -1) {
          printf("Nie udalo sie usunac pliku rekurencyjnie z '%s'\n",
                 file_path_dst);
          continue;
        }
      }
    }
  }
  closedir(dir);
  return 0;
}

int copy_file(const char *src_path, const char *dst_path, long long threshold) {

  int fSrc = open(src_path, O_RDONLY);
  if (fSrc == -1) {
    printf("Blad z otwarciem pliku zrodlowego '%s'\n", src_path);
    return -1;
  }

  struct stat statbuf;
  if (fstat(fSrc, &statbuf) == -1) {
    close(fSrc);
    return -1;
  }

  int fDst = open(dst_path, O_RDWR | O_CREAT | O_TRUNC, statbuf.st_mode);
  if (fDst == -1) {
    close(fSrc);
    printf("Blad z otwarciem pliku docelowego '%s'\n", dst_path);
    return -1;
  }

  if (statbuf.st_size >= 0 && statbuf.st_size <= threshold) {

    size_t buffSize = BUFF_SIZE_OPEN; // 8KB
    char *buff = malloc(buffSize);

    if (!buff) {
      printf("Blad z alokacja pamieci\n");
      close(fSrc);
      close(fDst);
      return -1;
    }

    ssize_t bytes;
    while ((bytes = read(fSrc, buff, buffSize)) > 0) {

      if (bytes != write(fDst, buff, (size_t)bytes)) {
        close(fSrc);
        close(fDst);
        free(buff);
        return -1;
      }
    }
    if (bytes == -1) {
      close(fSrc);
      close(fDst);
      free(buff);
      return -1;
    }

    free(buff);
    printf("Uzyto open/write\n");
  } else {
    void *src_mmap, *dst_mmap;
    src_mmap =
        mmap(NULL, (size_t)statbuf.st_size, PROT_READ, MAP_PRIVATE, fSrc, 0);

    if (src_mmap == MAP_FAILED) {
      printf("Nie udalo sie zmapowac zrodla\n");
      close(fSrc);
      close(fDst);
      return -1;
    }

    if (ftruncate(fDst, statbuf.st_size) == -1) {
      printf("Nie udalo sie poszerzyc pliku celu do zmapowania\n");
      close(fSrc);
      close(fDst);
      munmap(src_mmap, (size_t)statbuf.st_size);
      return -1;
    }

    dst_mmap = mmap(NULL, (size_t)statbuf.st_size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fDst, 0);

    if (dst_mmap == MAP_FAILED) {
      printf("Nie udalo sie zmapowac celu\n");
      munmap(src_mmap, (size_t)statbuf.st_size);
      close(fSrc);
      close(fDst);
      return -1;
    }

    memcpy(dst_mmap, src_mmap, (size_t)statbuf.st_size);

    munmap(src_mmap, (size_t)statbuf.st_size);
    munmap(dst_mmap, (size_t)statbuf.st_size);
    printf("Uzyto mmap\n");
  }

  close(fSrc);
  close(fDst);
  // zmiana czasu modyfikacji
  struct timespec times[2];
  times[0] = statbuf.st_atim; // Czas dostepu
  times[1] = statbuf.st_mtim; // Czas modyfikacji

  if (utimensat(AT_FDCWD, dst_path, times, 0) == -1) {
    printf("Nie udalo sie nadpisac czasu dla '%s'\n", dst_path);
    return -1;
  }
  return 0;
}

int synchronize(const char *src_path, const char *dst_path, int recursion,
                long long threshold) {
  DIR *dir;
  struct dirent *entry;

  dir = opendir(src_path);

  if (dir == NULL) {
    printf("Nie mozna otworzyc katalogu '%s'\n", src_path);
    return -1;
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

    if (lstat(file_path_src, &st_src) != 0) {
      continue;
    }

    if (S_ISREG(st_src.st_mode)) {
      if (lstat(file_path_dst, &st_dst) != 0 || !S_ISREG(st_dst.st_mode) ||
          st_src.st_mtim.tv_sec != st_dst.st_mtim.tv_sec ||
          st_src.st_mtim.tv_nsec != st_dst.st_mtim.tv_nsec) {
        if (copy_file(file_path_src, file_path_dst, threshold) == -1) {
          printf("Nie udalo sie skopiowac pliku z '%s' do '%s'\n",
                 file_path_src, file_path_dst);
          continue;
        }
      }
    } else if (recursion && S_ISDIR(st_src.st_mode)) {
      if (lstat(file_path_dst, &st_dst) != 0 || !S_ISDIR(st_dst.st_mode)) {
        mkdir(file_path_dst, st_src.st_mode & 0777);
      }
      if (synchronize(file_path_src, file_path_dst, recursion, threshold) ==
          -1) {
        printf("Nie udalo sie skopiowac pliku rekurencyjnie\n");
        continue;
      }
    }
  }
  closedir(dir);
  return 0;
}

int main(int argc, char *argv[]) {
  const char *src_path = NULL;
  const char *dst_path = NULL;

  int recursion = 0;
  long long mmap_threshold = DEFAULT_SIZE_MMAP;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      print_help(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-R") == 0) {
      recursion = 1;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (i + 1 < argc) {
        mmap_threshold = atoll(argv[++i]);
      }
      if (mmap_threshold == 0 || mmap_threshold < 0) {
        printf("Wartosc dla -t jest nieprawidlowa lub <= 0\n");
        return 1;
      }
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

  // 1. usuniecie nadmiaru z dst
  // jesli byl argument -R uwzglednia rowniez katalogi
  if (remove_file(src_path, dst_path, recursion) == -1) {
    printf("Nie udalo sie usunac nadmiaru plikow z katalogu docelowego '%s'\n",
           src_path);
    return 1;
  }

  // 2. synchronizacja z src do dst
  // jesli byl argument -R uwzglednia rowniez katalogi
  if (synchronize(src_path, dst_path, recursion, mmap_threshold) == -1) {
    printf("Nie udalo sie zsynchronizowac wszystkich plikow z katalogu "
           "zrodlowego '%s' do "
           "katalogu docelowego '%s'\n",
           src_path, dst_path);
    return 1;
  }

  return 0;
}

void print_help(const char *name) {
  printf("Demon synchronizujący dwa podkatalogi\n");
  printf("------------------------------\n");
  printf("Uzycie: %s [-h] [-R] [-t] <zrodlo> <cel>\n", name);
  printf("Parametry:\n");
  printf("  <zrodlo>    Katalog, z ktorego kopiujemy dane\n");
  printf("  <cel>       Katalog, ktory uaktualniamy i sprzatamy\n");
  printf("Opcje:\n");
  printf("  -R          Synchronizacja rekurencyjna (wchodzi w podkatalogi)\n");
  printf("  -t <bajty>  Prog wielkosci pliku w bajtach, powyzej ktorego "
         "demon\n");
  printf("              uzyje mmap zamiast read/write.\n");
  printf("              (Domyslna wartosc: 8388608 B = 8 MB)\n");
  printf("  -h          Wyswietla pomoc\n");
}
