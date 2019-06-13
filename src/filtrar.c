/*
 * filtrar: filtra por contenido los archivos del directorio indicado.
 *
 * Copyright (c) 2013,2017 Francisco Rosales <frosal@fi.upm.es>
 * Todos los derechos reservados.
 *
 * Publicado bajo Licencia de Proyecto Educativo Práctico
 * <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
 *
 * Queda prohibida la difusión total o parcial por cualquier
 * medio del material entregado al alumno para la realización 
 * de este proyecto o de cualquier material derivado de este, 
 * incluyendo la solución particular que desarrolle el alumno.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <ctype.h>
#include <signal.h>

#include "filtrar.h"

#define _GNU_SOURCE


/* ---------------- PROTOTIPOS ----------------- */ 
/* Esta funcion monta el filtro indicado y busca el simbolo "tratar"
   que debe contener, y aplica dicha funcion "tratar()" para filtrar
   toda la informacion que le llega por su entrada estandar antes
   de enviarla hacia su salida estandar. */
extern void filtrar_con_filtro(char* nombre_filtro);

/* Esta funcion lanza todos los procesos necesarios para ejecutar los filtros.
   Dichos procesos tendran que tener redirigida su entrada y su salida. */
void preparar_filtros(void);

/* Esta funcion recorrera el directorio pasado como argumento y por cada entrada
   que no sea un directorio o cuyo nombre comience por un punto '.' la lee y 
   la escribe por la salida estandar (que seria redirigida al primero de los 
   filtros, si existe). */
void recorrer_directorio(char* nombre_dir);

/* Esta funcion recorre los procesos arrancados para ejecutar los filtros, 
   esperando a su terminacion y recogiendo su estado de terminacion. */ 
void esperar_terminacion(void);

/* Desarrolle una funcion que permita controlar la temporizacion de la ejecucion
   de los filtros. */ 
extern void preparar_alarma(void);

/* Manejador de señales. */

void sigalrmhandler(void);

/* ---------------- IMPLEMENTACIONES ----------------- */ 
char** filtros;   /* Lista de nombres de los filtros a aplicar */
int    n_filtros; /* Tama~no de dicha lista */
pid_t* pids;      /* Lista de los PIDs de los procesos que ejecutan los filtros */



/* Funcion principal */
int main(int argc, char* argv[])
{
	/* Chequeo de argumentos */
	if(argc<2) 
	{
		/* Invocacion sin argumentos  o con un numero de argumentos insuficiente */
		fprintf(stderr,"Uso: %s directorio [filtro...]\n", argv[0]);
		exit(1);
	}

	filtros = &(argv[2]);                             /* Lista de filtros a aplicar */
	n_filtros = argc-2;                               /* Numero de filtros a usar */
	pids = (pid_t*)malloc(sizeof(pid_t)*n_filtros);   /* Lista de pids */

	preparar_alarma();

	if(n_filtros > 0)
		preparar_filtros();

	recorrer_directorio(argv[1]);

	esperar_terminacion();

	return 0;
}


void recorrer_directorio(char* nombre_dir)
{
	DIR *dir = NULL;
	struct dirent* ent;
	char fich[1024];
	char buff[4096];
	int fd;
	struct stat fileStat;

	/* Abrir el directorio */
	if ((dir = opendir(nombre_dir)) == NULL) {
		/* Tratamiento del error. */
		fprintf(stderr, "Error al abrir el directorio '%s'\n", nombre_dir);
		exit(1);
	}
	/* Recorremos las entradas del directorio */
	while((ent=readdir(dir))!=NULL)
	{
		/* Nos saltamos las que comienzan por un punto "." */
		if(ent->d_name[0]=='.')
			continue;

		/* fich debe contener la ruta completa al fichero */
		if (getcwd(fich,1024) == NULL){
			fprintf(stderr, "Error al obtener la ruta al fichero");
			exit(1);
		}
		strcat(fich,"/");
		strcat(fich,nombre_dir);
		strcat(fich,"/");
		strcat(fich,ent->d_name);

		/* Nos saltamos las rutas que sean directorios. */
		if(stat(fich,&fileStat) < 0){
			fprintf(stderr, "AVISO: No se puede stat el fichero '%s'!\n", fich);
			exit(1);
		}
		if(S_ISDIR(fileStat.st_mode))
			continue;

		/* Abrir el archivo. */
		if((fd = open(fich, O_RDONLY)) < 0){
			/* Tratamiento del error. */
			fprintf(stderr, "Error al abrir el fichero '%s'\n", fich);
			exit(1);
		}

		/* Cuidado con escribir en un pipe sin lectores! */
		 if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            fprintf(stderr, "Error al emitir el fichero '%s'\n", fich);
            close(fd);
			break;
		 }
		/* Emitimos el contenido del archivo por la salida estandar. */
		while(write(1,buff,read(fd,buff,4096)) > 0)
			continue;

		/* Cerrar. */
		if(close(fd) < 0){
			fprintf(stderr, "Error al cerrar el fichero '%s'\n", fich);
			exit(1);
		}
	}
	/* Cerrar. */
	if(closedir(dir) < 0){
		fprintf(stderr, "Error al cerrar el directorio");
		exit(1);
	}
	close(1);
	/* IMPORTANTE:
	 * Para que los lectores del pipe puedan terminar
	 * no deben quedar escritores al otro extremo. */
	// IMPORTANTE
	
}


void preparar_filtros(void){
	int fd[n_filtros][2];
	char ch = '.';
	char *ret;
	int i;
	for (i = 0; i < n_filtros; i++) {
		/* Tuberia hacia el hijo (que es el proceso que filtra). */
		if(pipe(fd[i]) < 0){
			fprintf(stderr, "Error al crear el pipe\n");
			exit(1);
		}
		/* Lanzar nuevo proceso */
		switch(pids[i] = fork())
		{
		case -1:
			/* Error. Mostrar y terminar. */
			fprintf(stderr, "Error al crear proceso %d\n", pids[i]);
			exit(1);
		case  0:
			/* Hijo: Redireccion y Ejecuta el filtro. */
			dup2(fd[i][0],0);
			close(fd[i][0]);
			close(fd[i][1]);
			ret = strrchr(filtros[i], ch);
			if (ret && !strcmp(ret, ".so")) {	/* El nombre termina en ".so" ? */
				/* SI. Montar biblioteca y utilizar filtro. */
				filtrar_con_filtro(filtros[i]);
			} else {	
				/* NO. Ejecutar como mandato estandar. */
				execlp(filtros[i], filtros[i], NULL);
				fprintf(stderr, "Error con el filtro");
				exit(1);
			}
			break;
		default:
			/* Padre: Redireccion */
			dup2(fd[i][1],1);
			close(fd[i][0]);
			close(fd[i][1]);
			break;
		}
	}
}


void imprimir_estado(char* filtro, int status)
{
	/* Imprimimos el nombre del filtro y su estado de terminacion */
	if(WIFEXITED(status))
		fprintf(stderr,"%s: %d\n",filtro,WEXITSTATUS(status));
	else
		fprintf(stderr,"%s: senyal %d\n",filtro,WTERMSIG(status));
}


void esperar_terminacion(void)
{
    int p, status;
    
    for(p=0;p<n_filtros;p++)
    {
	/* Espera al proceso pids[p] */
		if(waitpid(0, &status, 0) < 0){
			fprintf(stderr, "Error al esperar proceso %d\n", pids[p]);
			exit(1);
		}
	/* Muestra su estado. */
	imprimir_estado(filtros[p], status);
    }
}

void filtrar_con_filtro(char *nombre_filtro){
	void *handle;
	int (*tratar)(char*,char*,int);
	char buf_in[1024], buf_out[1024];
	char *error; 
	int filtrados, leidos = 0;

	handle = dlopen(nombre_filtro, RTLD_LAZY);
	error = dlerror();
	if(error != NULL){
		fprintf(stderr,"Error al abrir la biblioteca '%s'\n", nombre_filtro);
		exit(1);
	}
	tratar = dlsym(handle, "tratar");
	error = dlerror();
	if(error != NULL){
		fprintf(stderr, "Error al buscar el simbolo '%s' en '%s'\n", "tratar", nombre_filtro);
		exit(1);
	}
	while ((leidos = (int) read(0, buf_in, 1024)) > 0) {
        if ((filtrados = tratar(buf_in, buf_out, leidos)) == 0 )
             break;
        write(1, buf_out, sizeof(char) * filtrados);
	}
	if(dlclose(handle) < 0){
		fprintf(stderr, "Error al cerrar la biblioteca '%s'\n", nombre_filtro);
		exit(1);
	}
	exit(0);
}

void preparar_alarma(void){
	char* ent;
	int tiempo;
	char* ptr;
	if((ent = getenv("FILTRAR_TIMEOUT")) != NULL){
		if (!isdigit(ent[0])) {
            fprintf(stderr, "Error FILTRAR_TIMEOUT no es entero positivo: '%s'\n", ent);
			exit(1);
		}
		tiempo = strtol(ent, &ptr, 10);
		signal(SIGALRM, sigalrmhandler);
		alarm(tiempo);
		fprintf(stderr, "AVISO: La alarma vencera tras %d segundos!\n", tiempo);
	}
}

void sigalrmhandler(void){
	int i;
	fprintf(stderr,"AVISO: La alarma ha saltado!\n");
	if (n_filtros > 0) {
        for (i = 0; i < n_filtros; i++) {
            if (kill(pids[i], 0) == 0) {
                if ((kill(pids[i], SIGKILL)) < 0 ) {
                    fprintf(stderr, "Error al intentar matar proceso %d\n", pids[i]);
                    exit(1);
                }
            }
		}
	}
	esperar_terminacion();
	exit(0);
}
