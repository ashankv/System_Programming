/**
 * MapReduce
 * CS 241 - Fall 2017
 */

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

int map_count;
char* input_file;
char* output_file;
int fildes[10000][2];

int main(int argc, char **argv) {
	if (argc != 6) {
		print_usage();
		exit(-1);
	}
	input_file = argv[1];
	//printf("input file is %s\n", input_file);
	output_file = argv[2];
    sscanf(argv[5],"%d",&map_count);
    if (map_count > 10) map_count = 10;
     
   	int pipe_reducer[2];
   	pipe(pipe_reducer);
   
    pid_t mappers[10000];
   	
   	 
    for (int i = 0; i < map_count; i++) {
		mappers[i] = fork();
    	if (mappers[i] == 0) {
    		// I am a mapper
    		pipe(fildes[i]);

    		pid_t my_child = fork();
    		if (my_child == 0) {
    			// mapper create a son to split
    			//char dis = '0' + i;
    			char dis_num[1000]; memset(dis_num,0, 1000);
    			sprintf(dis_num, "%d", i);
    			char t_num[1000]; memset(t_num,0, 1000);
    			sprintf(t_num, "%d", map_count);
    			close(fildes[i][0]);
    			dup2(fildes[i][1],1);
    			int a = execlp("./splitter","./splitter", input_file, t_num, dis_num , NULL);
    			fprintf(stderr, "a is %d\n", a); 
    			
    		} else {
    		//initial mapper
    			waitpid(my_child, NULL, 0);
				//read from fildes[i][0]
				
				close(fildes[i][1]);
				dup2(fildes[i][0],0);
				close(pipe_reducer[0]);
				dup2(pipe_reducer[1],1);
				int b = 0;
				//fprintf(stderr, "b is before %d\n", b);
				b = execlp(argv[3],argv[3], NULL); //map !!
				//fprintf(stderr, "b is after %d\n", b);
				print_nonzero_exit_status(argv[3], b);
				exit(b);
    		}	
    	} else {
    		continue;
    	}
    }
    
    pid_t p = fork();
    if (p == 0) {
    	// child
    	//fprintf(stderr, "IN CHILD\n");
    	//
    	//reducer
    	close(pipe_reducer[1]);
    	dup2(pipe_reducer[0],0); 
    	//close(1);
    	int sav_stdout = dup(1);
    	int fd_open = open(output_file, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    	dup2(fd_open, 1);
    	int c = execlp(argv[4], argv[4], NULL);
    	dup2(sav_stdout, 1);
    	close(sav_stdout);
    	print_nonzero_exit_status(argv[4], c);
    	exit(c);
    } else {
    	close(pipe_reducer[1]);
    	close(pipe_reducer[0]);
   		int stp;
   		waitpid(p , &stp, 0);
   		int exit_p = WEXITSTATUS(stp);

   		
   		
   		
   		
    	FILE* fd_out = fopen(output_file, "r");
    	int lines = 0;
    	char* buffer = NULL;
    	size_t n = 0;
    	while ( getline(&buffer, &n, fd_out) != -1 ) {
    		lines++;
    		free(buffer);
    		buffer = NULL;
    	}
    	if (buffer) free(buffer);
    	fclose(fd_out);
    	//fprintf(stderr, "lines = %d\n", lines);
    	int ct = 0;
    	while (1) {
    		for (int i = 0; i < map_count; i++) {
    			int status_map;
    			waitpid(mappers[i], &status_map, WNOHANG);
    			if (WIFEXITED(status_map)) {
    				ct++;
    				int exit_val = WEXITSTATUS(status_map);
    				if (exit_val != 0) print_nonzero_exit_status(argv[3], exit_val);
    			}
    		}
    		int status_p;
    		waitpid(p, &status_p, WNOHANG);
    		if (WIFEXITED(status_p)) {
    			ct++;
    			//int exit_val = WEXITSTATUS(status_p);
    			//fprintf(stderr, "bad num = %d\n", exit_val);
    			//if (exit_val != 0) print_nonzero_exit_status(argv[4], exit_val);
    		} 
    		if (ct == map_count + 1) break;	
    	}
    	
		//fprintf(stderr, "bad num = %d\n", exit_p);
    	if (exit_p != 0) print_nonzero_exit_status(argv[4], exit_p);
 
    }
    

    // Create an input pipe for each mapper.
	
    // Create one input pipe for the reducer.

    // Open the output file.

    // Start a splitter process for each mapper.

    // Start all the mapper processes.

    // Start the reducer process.

    // Wait for the reducer to finish.

    // Print nonzero subprocess exit codes.

    // Count the number of lines in the output file.

    return 0;
}
