# COMPILATION
'gcc -Wall -Wextra -pedantic -Wconversion main.c -o main'

# TEST
sudo docker run -it --rm -v $(pwd):/projekt gcc:latest bash

# CHECK PID
'pidof main'

# SEND SIGNAL
kill -SIGUSR1 PID

# HELP
./main -h
