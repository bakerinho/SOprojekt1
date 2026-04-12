#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  printf("Start\n");
  pid_t id = fork();
  if (id > 0) {
    // parent
    printf("Parent, id: %d\n", getpid());
  } else if (id == 0) {
    // child
    printf("Child, id: %d\n", getpid());
  } else {
    printf("Fork failed\n");
    return 1;
  }

  printf("Both\n");
  while (1) {
    if (id > 0) {
      return 0;
    } else {
      sleep(1);
    }
  }
  return 0;
}
