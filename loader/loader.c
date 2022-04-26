/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "exec_parser.h"
#include <math.h>
#include <errno.h>
#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);					\
		}							\
	} while (0)

static so_exec_t *exec;
static int pageSize;
static struct sigaction old_action;
char* file_name;

int min(unsigned int a, unsigned int b) {
	return a < b ? a : b;
}
char* get_input(unsigned int, unsigned int);

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	char *addr;
	int rc;
	/** AM PRELUAT BUCATI DE COD DIN LAB06 DIN 5-PROT/TODO2 PENTRU TRATAREA SEMNALULUI SIGSEGV*/

	

	/*  - check if the signal is SIGSEGV */
	if (signum != SIGSEGV) {
		old_action.sa_sigaction(signum, info, context);
		return;
	}

	/**
	 * Obtain from siginfo_t the memory location
	 * which caused the page fault
	 */
	addr = (char *)info->si_addr;

	/* - Obtain the page which caused the page fault */
	int page ;
	int ok = 0;
	int num_segments = exec->segments_no;
	so_seg_t segment;
	for (int i = 0; i < num_segments; i++) {
		/* caut in ce segment s-a produs PAGE FAULT ul */
		if ((uintptr_t) addr >= exec->segments[i].vaddr && 
			(uintptr_t) addr <= (exec->segments[i].vaddr + exec->segments[i].mem_size)) {
				/* Sunt in segmentul in care s a produs PAGE FAULT-ul.
				 * Acum determin pagina in case s a produs PAGE FAULT */

				segment = exec->segments[i]; /* Salvez segmentul in variabila "segment" */
				/* pag = [(adresa_page_fault - adresa_inceput_segment) / dim_pagina] ([] -> parte intreaga)   */
				page = ((unsigned int)addr - (unsigned  int)exec->segments[i].vaddr) / pageSize; 
				ok = 1;
				break;
		}
	}

	if (ok == 1) {
		int* isMapped = (int*) segment.data;
		/* Pagina nu este mapata */
		if (isMapped[page] == 0) {
			isMapped[page] = 1;

			printf("1\n");
			
			int dataSize;
			
			/* offsetul din fisier */
			int fileOff = segment.offset + (page * pageSize);
			
			if (pageSize * page > segment.file_size) /*daca sunt in ZERO */
				dataSize = 0; /* NU */
			else
				dataSize = min(pageSize, segment.file_size - (pageSize * page)); /* DA */

			char* fromFileInput;

			if (dataSize > 0) { 
				fromFileInput = get_input(fileOff, dataSize);

				if (dataSize < pageSize) /*ZERO*/
					memset((void*) (fromFileInput + dataSize), 0, pageSize - dataSize);	
			} else
				fromFileInput = calloc(pageSize, sizeof(char));

			unsigned int page_addr = segment.vaddr + page * pageSize;

			char *mapped = mmap((void *)page_addr, pageSize, 
				PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED | MAP_PRIVATE, -1, 0);
			DIE(mapped == (char *)-1, "mmap");

			/* ACTUALIZEZ PAGINA mapped CU DATELE DIN FISIER */
			memcpy(mapped, fromFileInput, pageSize);
			free(fromFileInput);
			rc = mprotect(mapped, pageSize, segment.perm);
			DIE(rc < 0, "mprotect");

		} else {
			/* ACCESEZ O PAGINA MAPATA DAR NU AM PERMISIUNI */
			old_action.sa_sigaction(signum, info, context);
			return;
		}
	}
	/* Daca nu s a gasit adresa pfault ului in interiorul unui segment se trateaza semnalul
	 * SEGSIGV normal */
	if (ok == 0) {
		old_action.sa_sigaction(signum, info, context);
		return;
	}

}
char* get_input(unsigned int offset, unsigned int size)
{
	char* data = malloc(sizeof(char) * pageSize);
	DIE(data == NULL, "MALLOC");

	int fd = open(file_name, O_RDONLY);
	DIE(fd < 0, "open");


	off_t cursor_pos = lseek(fd, offset, SEEK_SET);
	DIE(cursor_pos < 0, "lseek");

	int bytes_read = read(fd, data, size);
	DIE(bytes_read < 0, "read");

	int close_ret = close(fd);
	DIE(close_ret < 0, "close");

	return data;
	
}

static void set_signal(void)
{
	struct sigaction action;
	int rc;

	action.sa_sigaction = segv_handler;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGSEGV);
	action.sa_flags = SA_SIGINFO;

	rc = sigaction(SIGSEGV, &action, &old_action);
	DIE(rc == -1, "sigaction");
}



int so_init_loader(void)
{
	pageSize = getpagesize();
	set_signal();

	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;
	
	
	file_name = strdup(path);

	for (int i = 0; i < exec->segments_no; i++) {
		/* in fiecare segment adaug in data un vector pt mapati sau nemapati */
		int numPages = exec->segments[i].mem_size / pageSize;
		exec->segments[i].data = (int*) calloc(numPages, sizeof(int));	
	}
	so_start_exec(exec, argv);

	return 0;
}
