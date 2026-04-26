#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 4096
#define BUFF_SIZE_OPEN 8192
#define DEFAULT_SIZE_MMAP (8 * 1024 * 1024)

#define SLEEP_INTERVAL (5 * 60) // 5 min

typedef struct {
  char src_path[MAX_PATH_LENGTH];
  char dst_path[MAX_PATH_LENGTH];
  int recursion;
  long long mmap_threshold;
  int sleep_interval;
} Config;

void print_help(const char *name);

int // returns 0 on success, -1 on error
delete_recursive(const char *path) {
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
      syslog(LOG_ERR, "Nie znaleziono pliku docelowego '%s'", file_path);
      continue;
    }
    if (S_ISDIR(statbuf.st_mode)) {
      delete_recursive(file_path);
    } else {
      unlink(file_path);
      syslog(LOG_INFO, "Usunieto plik: %s", file_path);
    }
  }
  closedir(dir);
  // usuniecie folderu w ktorym usunieto wszystkie pliki
  rmdir(path);
  syslog(LOG_INFO, "Usunieto katalog: %s", file_path);
  return 0;
}

int // returns 0 on success, -1 on error
remove_file(const char *src_path, const char *dst_path, int recursion) {

  DIR *dir;
  struct dirent *entry;

  dir = opendir(dst_path);

  if (dir == NULL) {
    syslog(LOG_ERR, "Nie mozna otworzyc katalogu '%s'", dst_path);
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
      syslog(LOG_ERR, "Nie znaleziono pliku docelowego '%s'", file_path_dst);
      continue;
    }
    if (S_ISREG(st_dst.st_mode)) {
      if (lstat(file_path_src, &st_src) != 0 || !S_ISREG(st_src.st_mode)) {

        unlink(file_path_dst);
        syslog(LOG_INFO, "Usunieto plik: %s", file_path_dst);
      }
    } else if (recursion && S_ISDIR(st_dst.st_mode)) {
      if (lstat(file_path_src, &st_src) != 0 || !S_ISDIR(st_src.st_mode)) {
        if (delete_recursive(file_path_dst) == -1) {
          syslog(LOG_ERR, "Nie udalo sie usunac pliku rekurencyjnie z '%s'",
                 file_path_dst);
          continue;
        }
      } else {
        if (remove_file(file_path_src, file_path_dst, recursion) == -1)
          continue;
      }
    }
  }
  closedir(dir);
  return 0;
}

int // returns 0 on success, -1 on error
copy_read_write(int fSrc, int fDst) {
  size_t buffSize = BUFF_SIZE_OPEN; // 8KB
  char *buff = malloc(buffSize);

  if (!buff) {
    syslog(LOG_ERR, "Blad z alokacja pamieci bufora");
    return -1;
  }

  ssize_t bytes;
  while ((bytes = read(fSrc, buff, buffSize)) > 0) {
    if (bytes != write(fDst, buff, (size_t)bytes)) {
      free(buff);
      return -1; // Blad zapisu
    }
  }

  free(buff);

  if (bytes == -1) {
    return -1;
  }

  // syslog(LOG_INFO, "Uzyto open/write");
  return 0;
}

int // returns 0 on success, -1 on error
copy_mmap(int fSrc, int fDst, size_t file_size) {
  void *src_mmap = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fSrc, 0);
  if (src_mmap == MAP_FAILED) {
    syslog(LOG_ERR, "Nie udalo sie zmapowac zrodla");
    return -1;
  }

  ssize_t written = write(fDst, src_mmap, file_size);
  munmap(src_mmap, file_size);

  if (written == -1 || (size_t)written != file_size) {
    syslog(LOG_ERR, "Blad zapisu zmapowanego pliku");
    return -1;
  }

  return 0;
}

int // returns 0 on success, -1 on error
copy_file(const char *src_path, const char *dst_path, long long threshold) {

  int fSrc = open(src_path, O_RDONLY);
  if (fSrc == -1) {
    syslog(LOG_ERR, "Blad z otwarciem pliku zrodlowego '%s'", src_path);
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
    syslog(LOG_ERR, "Blad z otwarciem pliku docelowego '%s'", dst_path);
    return -1;
  }

  int copy_status = 0;

  // Decyzja, w jaki sposób będzie kopiowane
  if (statbuf.st_size >= 0 && statbuf.st_size <= threshold) {
    copy_status = copy_read_write(fSrc, fDst);
  } else {
    copy_status = copy_mmap(fSrc, fDst, (size_t)statbuf.st_size);
  }

  close(fSrc);
  close(fDst);

  // Późniejsze wyjście z funkcji, żeby pozamykać pliki
  if (copy_status == -1) {
    return -1;
  }

  // Zmiana czasu modyfikacji
  struct timespec times[2];
  times[0] = statbuf.st_atim; // Czas dostepu
  times[1] = statbuf.st_mtim; // Czas modyfikacji

  if (utimensat(AT_FDCWD, dst_path, times, 0) == -1) {
    syslog(LOG_ERR, "Nie udalo sie nadpisac czasu dla '%s'", dst_path);
    return -1;
  }

  syslog(LOG_INFO, "Skopiowano plik: %s", dst_path);
  return 0;
}

int // returns 0 on success, -1 on error
synchronize(const char *src_path, const char *dst_path, int recursion,
            long long threshold) {
  DIR *dir;
  struct dirent *entry;

  dir = opendir(src_path);

  if (dir == NULL) {
    syslog(LOG_ERR, "Nie mozna otworzyc katalogu '%s'", src_path);
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
          syslog(LOG_ERR, "Nie udalo sie skopiowac pliku z '%s' do '%s'",
                 file_path_src, file_path_dst);
          continue;
        }
      }
    } else if (recursion && S_ISDIR(st_src.st_mode)) {
      if (lstat(file_path_dst, &st_dst) != 0 || !S_ISDIR(st_dst.st_mode)) {
        mkdir(file_path_dst, st_src.st_mode & 0777);
        syslog(LOG_INFO, "Utworzono katalog: %s", file_path_dst);
      }
      if (synchronize(file_path_src, file_path_dst, recursion, threshold) ==
          -1) {
        syslog(LOG_ERR, "Nie udalo sie skopiowac pliku rekurencyjnie");
        continue;
      }
    }
  }
  closedir(dir);
  return 0;
}

int // returns 0 on success, -1 on error
create_daemon() {
  pid_t pid;

  // Pierwszy fork odłącza program od terminala
  // pozwalając mu działać w tle samemu.
  pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    exit(EXIT_SUCCESS);

  // Utworzenie nowej sesji. Proces staje się liderem nowej sesji i grupy
  // procesów, całkowicie odcinając się od terminala, z którego został
  // uruchomiony.
  if (setsid() < 0)
    return -1;

  // 3. Ignorowanie sygnałów kontrolnych z systemu.
  // SIGCHLD - ochrona przed powstawaniem procesów 'zombie'.
  // SIGHUP - gwarancja że zamknięcie okna terminala nie zabije procesu.
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  // 4. Drugi fork odbiera procesowi status lidera sesji.
  // Daje to gwarancję, że proces już nigdy przypadkowo nie podepnie
  // się pod żaden nowy terminal
  pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    exit(EXIT_SUCCESS);

  // 5. Demony nie korzystają z ekranu ani klawiatury.
  // Przekierowanie standardowych strumieni (stdin, stdout, stderr) do /dev/null
  // zapobiega błędom IO, gdyby jakakolwiek funkcja próbowała coś wypisać na
  // ekran.
  int fd_null = open("/dev/null", O_RDWR);
  if (fd_null == -1) {
    return -1;
  }

  dup2(fd_null, STDIN_FILENO);
  dup2(fd_null, STDOUT_FILENO);
  dup2(fd_null, STDERR_FILENO);
  close(fd_null);

  // 6. Zerowanie maski uprawnień (umask).
  // Gwarantuje to, że pliki tworzone przez demona będą miały dokładnie takie
  // uprawnienia, o jakie poprosi w kodzie, bez dziedziczenia ograniczeń po
  // systemie użytkownika.
  umask(0);

  // 7. Zmiana katalogu roboczego na główny ('/').
  // Zapobiega to sytuacji, w której demon zablokowałby możliwość odmontowania
  // dysku/katalogu, z którego został uruchomiony.
  if (chdir("/") == -1) {
    return -1;
  }

  return 0;
}

void wake_up_handler(int signum) {
  syslog(LOG_INFO, "Otrzymano sygnal SIGUSR1. wznawianie synchronizacji...");
}

int init_config(int argc, char *argv[], Config *config) {
  const char *raw_src_path = NULL;
  const char *raw_dst_path = NULL;

  config->recursion = 0;
  config->mmap_threshold = DEFAULT_SIZE_MMAP;
  config->sleep_interval = SLEEP_INTERVAL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      print_help(argv[0]);
      exit(0); // Od razu wychodzimy z programu
    } else if (strcmp(argv[i], "-R") == 0) {
      config->recursion = 1;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (i + 1 < argc)
        config->mmap_threshold = atoll(argv[++i]);
      if (config->mmap_threshold <= 0) {
        printf("Blad: Wartosc dla -t jest nieprawidlowa lub <= 0\n");
        return -1;
      }
    } else if (strcmp(argv[i], "-s") == 0) {
      if (i + 1 < argc)
        config->sleep_interval = atoi(argv[++i]);
      if (config->sleep_interval <= 0) {
        printf("Blad: Wartosc dla -s jest nieprawidlowa lub <= 0\n");
        return -1;
      }
    } else if (raw_src_path == NULL) {
      raw_src_path = argv[i];
    } else if (raw_dst_path == NULL) {
      raw_dst_path = argv[i];
    } else {
      printf("Blad: Podano zbyt duzo argumentow\n");
      return -1;
    }
  }

  if (raw_src_path == NULL || raw_dst_path == NULL) {
    printf(
        "Wymagane sa dwa argumenty: <katalog zrodlowy> <katalog docelowy>\n");
    print_help(argv[0]);
    return -1;
  }

  struct stat statbuf;
  if (lstat(raw_src_path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
    printf(
        "Blad: Sciezka zrodlowa '%s' nie istnieje lub nie jest katalogiem.\n",
        raw_src_path);
    return -1;
  }
  if (lstat(raw_dst_path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
    printf(
        "Blad: Sciezka docelowa '%s' nie istnieje lub nie jest katalogiem.\n",
        raw_dst_path);
    return -1;
  }

  if (realpath(raw_src_path, config->src_path) == NULL ||
      realpath(raw_dst_path, config->dst_path) == NULL) {
    printf("Blad: Nie udalo sie znalezc sciezki bezwzglednej.\n");
    return -1;
  }

  size_t src_len = strlen(config->src_path);
  size_t dst_len = strlen(config->dst_path);

  if (strcmp(config->src_path, config->dst_path) == 0) {
    printf("Blad: Katalog zrodlowy i docelowy to to samo miejsce!\n");
    return -1;
  }
  if ((strncmp(config->src_path, config->dst_path, src_len) == 0) &&
      config->dst_path[src_len] == '/') {
    printf(
        "Blad: Katalog docelowy nie moze sie znajdowac wewnatrz zrodlowego\n");
    return -1;
  }
  if ((strncmp(config->dst_path, config->src_path, dst_len) == 0) &&
      config->src_path[dst_len] == '/') {
    printf(
        "Blad: Katalog zrodlowy nie moze sie znajdowac wewnatrz docelowego\n");
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  Config config;

  if (init_config(argc, argv, &config) == -1) {
    return 1;
  }

  if (create_daemon() == -1) {
    return 1;
  }

  openlog("demon_synch", LOG_PID | LOG_CONS, LOG_DAEMON);
  syslog(LOG_INFO, "Demon uruchomiony poprawnie. Sciezki: %s -> %s",
         config.src_path, config.dst_path);

  if (signal(SIGUSR1, wake_up_handler) == SIG_ERR) {
    syslog(LOG_ERR, "Blad rejestracji sygnalu SIGUSR1");
    closelog();
    return 1;
  }

  while (1) {
    syslog(LOG_INFO, "ROZPOCZECIE SYNCHRONIZACJI");

    if (remove_file(config.src_path, config.dst_path, config.recursion) == -1) {
      syslog(LOG_ERR, "Nie udalo sie usunac nadmiaru plikow z '%s'",
             config.dst_path);
    }

    if (synchronize(config.src_path, config.dst_path, config.recursion,
                    config.mmap_threshold) == -1) {
      syslog(LOG_ERR, "Blad synchronizacji z '%s' do '%s'", config.src_path,
             config.dst_path);
    }

    syslog(LOG_INFO,
           "ZAKONCZENIE SYNCHRONIZACJI. Demon idzie spac na %d sekund.",
           config.sleep_interval);
    sleep((unsigned int)config.sleep_interval);
  }

  closelog();
  return 0;
}

void print_help(const char *name) {
  printf("Demon synchronizujący dwa podkatalogi\n");
  printf("---------------------------------------\n");
  printf("Uzycie: %s [-h] [-R] [-t <bajty>] [-s <sekundy>] <zrodlo> <cel>\n",
         name);
  printf("Parametry:\n");
  printf("  <zrodlo>        Katalog, z ktorego kopiujemy dane\n");
  printf("  <cel>           Katalog, ktory uaktualniamy i sprzatamy\n");
  printf("Opcje:\n");
  printf("  -R              Synchronizacja rekurencyjna (wchodzi w "
         "podkatalogi)\n");
  printf("  -t <bajty>      Prog wielkosci pliku w bajtach, powyzej ktorego "
         "demon\n");
  printf("                  uzyje mmap zamiast read/write.\n");
  printf("                  (Domyslna wartosc: 8388608 B = 8 MB)\n");
  printf("  -s <sekundy>    Czas usypiania demona miedzy cyklami "
         "synchronizacji.\n");
  printf("                  (Domyslna wartosc: 300 s = 5 minut)\n");
  printf("  -h              Wyswietla pomoc\n");
}
