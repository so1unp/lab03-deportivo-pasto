#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t terminar = 0;

void manejar_sigterm(int sig) { 
  (void)sig; //Esto es porque no se usa la variable sig.
  terminar = 1; 
}

int busywork(void) {
  struct tms buf;
  for (;;) {
    times(&buf);
    if (terminar)
      break; // Sale del bucle al recibir la señal
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf("Uso: %s <num_hijos> <segundos> <cambiar_prio 0|1>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int n_hijos = atoi(argv[1]);
  unsigned segundos = (unsigned)atoi(argv[2]);    //Esto es porque no podemos pasar segundos negativos pero atoi si puede tener numeros negativos.
  int cambiar_prio = atoi(argv[3]);

  pid_t pids[n_hijos];

  for (int i = 0; i < n_hijos; i++) {
    pids[i] = fork();

    if (pids[i] == 0) { // Lógica del HIJO
      // Configurar la señal SIGTERM
      struct sigaction sa;
      sa.sa_handler = manejar_sigterm;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags = 0;
      sigaction(SIGTERM, &sa, NULL);

      // Cambiar prioridad si se solicitó (valores de 0 a 19)
      if (cambiar_prio) {
        setpriority(PRIO_PROCESS, 0, i);
      }

      busywork(); // Consume CPU hasta recibir SIGTERM

      // Al terminar busywork, obtenemos estadísticas
      struct rusage usage;
      getrusage(RUSAGE_SELF, &usage);
      int prio = getpriority(PRIO_PROCESS, 0);

      // Tiempo de CPU de usuario
      long tiempo = usage.ru_utime.tv_sec;

      printf("Child %d (nice %2d):\t%3li\n", getpid(), prio, tiempo);
      exit(EXIT_SUCCESS);
    }
  }

  // Lógica del PADRE
  if (segundos > 0) {
    sleep(segundos);
    for (int i = 0; i < n_hijos; i++) {
      kill(pids[i], SIGTERM); // Envía la señal a cada hijo
    }
  } else {
    // Si segundos es 0, el padre espera para siempre (o hasta un Ctrl+C)
    for (;;)
      pause();
  }

  // Esperar a que todos los hijos terminen para no dejar procesos zombie
  for (int i = 0; i < n_hijos; i++)
    wait(NULL);

  return 0;
}
