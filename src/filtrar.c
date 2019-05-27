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
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#include "filtrar.h"


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

/* Manejador de señales para el SIGPIPE de recorrer_directorio.*/
void sighandler(int signum);

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
		printf("Uso correcto: %s <directorio> [filtro...]\n", argv[0]);
		exit(0);
	}

	filtros = &(argv[2]);                             /* Lista de filtros a aplicar */
	n_filtros = argc-2;                               /* Numero de filtros a usar */
	pids = (pid_t*)malloc(sizeof(pid_t)*n_filtros);   /* Lista de pids */

//	preparar_alarma();

//	preparar_filtros();

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
		perror("opendir");
		exit(0);
	}
	/* Recorremos las entradas del directorio */
	while((ent=readdir(dir))!=NULL)
	{
		/* Nos saltamos las que comienzan por un punto "." */
		if(ent->d_name[0]=='.')
			continue;

		/* fich debe contener la ruta completa al fichero */
		if (getcwd(fich,1024) == NULL){
			perror("getcwd");
			exit(0);
		}
		strcat(fich,"/");
		strcat(fich,nombre_dir);
		strcat(fich,"/");
		strcat(fich,ent->d_name);

		/* Nos saltamos las rutas que sean directorios. */
		if(stat(fich,&fileStat) < 0){
			perror("stat");
			exit(0);
		}
		if(S_ISDIR(fileStat.st_mode))
			continue;

		/* Abrir el archivo. */
		if((fd = open(fich, O_RDONLY)) < 0){
			/* Tratamiento del error. */
			perror("open");
			exit(0);
		}

		/* Cuidado con escribir en un pipe sin lectores! */
		signal(SIGPIPE,sighandler);

		/* Emitimos el contenido del archivo por la salida estandar. */
		while(write(1,buff,read(fd,buff,4096)) > 0)
			continue;

		/* Cerrar. */
		if(close(fd) < 0){
			perror("close");
			exit(0);
		}
	}
	/* Cerrar. */
	if(closedir(dir) < 0){
		perror("closedir");
		exit(0);
	}

	/* IMPORTANTE:
	 * Para que los lectores del pipe puedan terminar
	 * no deben quedar escritores al otro extremo. */
	// IMPORTANTE
	
}


void preparar_filtros(void)
{
	int p;


	{
		/* Tuberia hacia el hijo (que es el proceso que filtra). */

		/* Lanzar nuevo proceso */
		switch(p)
		{
		case -1:
			/* Error. Mostrar y terminar. */

		case  0:
			/* Hijo: Redireccion y Ejecuta el filtro. */

		//	if ()	/* El nombre termina en ".so" ? */
			{	/* SI. Montar biblioteca y utilizar filtro. */
		//		filtrar_con_filtro(filtros[p]);
			}
		//	else
			{	/* NO. Ejecutar como mandato estandar. */
			}
		//
		default:
			/* Padre: Redireccion */
			break;
		}
	}
}


void imprimir_estado(char* filtro, int status)
{
	/* Imprimimos el nombre del filtro y su estado de terminacion */
	if(WIFEXITED(status))
		printf(stderr,"%s: %d\n",filtro,WEXITSTATUS(status));
	else
		printf(stderr,"%s: senal %d\n",filtro,WTERMSIG(status));
}


void esperar_terminacion(void)
{
    int p;
    
    for(p=0;p<n_filtros;p++)
    {
	/* Espera al proceso pids[p] */

	/* Muestra su estado. */

    }
}

void sighandler(int signum) {
   fprintf(stderr,"Error al transmitir el fichero, nadie leyendo\n");
   exit(0);
}
