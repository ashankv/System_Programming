/**
 * Networking
 * CS 241 - Fall 2017
 */
#include "common.h"
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

char **parse_args(int argc, char **argv);
verb check_args(char **args);

struct addrinfo *result;

int fd_connect_to_server(char* host, char* port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; /* IPv4 only */
	hints.ai_socktype = SOCK_STREAM; /* TCP */
    
    int s = getaddrinfo(host, port, &hints, &result);
    if (s != 0) {
    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    	exit(-1);
    }

	if (connect(sock_fd, result->ai_addr, result->ai_addrlen) == -1) {
    	perror("connect");
       exit(-1);
    }
    return sock_fd;
}

char* sousa_get_str(char* str, size_t* size) {
	
	if (strstr(str, "OK") == str) {
		//sscanf("%zu", str + 3, size);
		for (int i = 14; i >= 0; i -= 2) {
			*size += str[i / 2 + 3] << (i * 4);
		} 
		//printf("size = %zu\n", *size);
		return str + 3 + 8;
	} else if (strstr(str, "ERROR") == str) {
		return NULL;
	} else {
		return (char*)(-1);
	}
}

char large_buffer[1000000];

void change_size_to_array(unsigned char* arr, size_t size) {
	//printf("size is %lx\n", size);
	int i;
	for (i = 0; i < 8; i++) {
		arr[i] = 0xFF & size;
		size = size >> 8;
		//fprintf(stderr, "%hhx\n",arr[i]);
	}
	
}

int main(int argc, char **argv) {
    // Good luck!
    //fprintf(stderr, "%s\n", argv[1]);
    if (argc < 3) exit(-1);
    char port[100]; memset(port, 0, 100);
    char host[100]; memset(host, 0, 100);
    char* port_begin = strstr(argv[1], ":");
    strcpy(port, port_begin + 1);
    *port_begin = 0;
    strcpy(host, argv[1]);
    *port_begin = ':';
    
    //char** stuff = parse_args(argc, argv);
    verb do_what = check_args(argv);
	//fprintf(stderr, "host is %s, port is %s\n", host, port);
	int server_fd = fd_connect_to_server(host, port);
	    
    if (do_what == GET) {
    	char* remote_name = argv[3];
    	char* local_name = argv[4];
    	//printf("GET %s\n", remote_name);
    	int local_fd = open(local_name, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    	dprintf(server_fd, "GET %s\n", remote_name);
    	shutdown(server_fd, SHUT_WR);
    	char buffer_get_header[256]; memset(buffer_get_header, 0, 256);
        int first_read = 0;
        while (1) {
            int errno_saved = errno;
            first_read = read(server_fd, buffer_get_header, 256);
            if (first_read >= 0) break;
            if (first_read == -1 && errno == EAGAIN) {
                errno = errno_saved;
                continue;
            } else {
                fprintf(stderr, "READ ERR\n");
                exit(1);
            }
            errno = errno_saved;
        }
        size_t size = 0;
        size_t real_size = first_read - 11;
        char* info = sousa_get_str(buffer_get_header, &size);
        if (info && info != (char*)-1) {
            // OK
            write(local_fd, info, first_read - 11);
            char tp[4096]; memset(tp, 0, 4096);
            while (1) {
                memset(tp, 0, 4096);
                int errno_saved = errno;
                int second_read = read(server_fd, tp, 4096);
                if (second_read == 0) break;
                if (second_read > 0) {
                    write(local_fd, tp, second_read);
                    real_size += second_read;
                } else {
                    if (first_read == -1 && errno == EAGAIN) {
                        errno = errno_saved;
                        continue;
                    }
                }
                errno = errno_saved;
            }
            if (real_size < size) {
                print_too_little_data();
                //exit(-1);
            } else if (real_size > size) {
                print_received_too_much_data();
            }

        } else if (info == NULL) {
            //error
            char* t = strstr(buffer_get_header, "\n");
            t = strstr(t + 1, "\n");
            char tp = *t;
            *t = 0;
            print_error_message(buffer_get_header + 6);
            *t = tp;
        } else {
            print_invalid_response();
            //exit(-1);
        }
        /*int num = 0;
    	char* dis = large_buffer;
    	size_t total_bytes_read = 0;
    	while ((num = read(server_fd, dis, 1) > 0)) {
    		//fprintf(stderr, "this str is %s\n", buf);
    		dis = dis + num;
    		total_bytes_read += num;
    		//write(local_fd, buf, num);
    		//for (int i = 0; i < num; i++) {fprintf(stderr, "%c,", large_buffer[i]);}
    	}
        */
    	//if (info) for (int i = 0; i < 30; i++) {fprintf(stderr, "%c", large_buffer[i] >= 48 ? large_buffer[i] : '~');}
    	//printf("\n");
    } else if (do_what == PUT) {
    	char* remote_name = argv[3];
    	char* local_name = argv[4];
    	FILE* loc_file = fopen(local_name, "rb");
    	if (!loc_file) exit(-1);
    	struct stat st;
		stat(local_name, &st);
		size_t size = st.st_size;
		unsigned char size_arr[8];
		change_size_to_array(size_arr, size);
        fprintf(stderr, "size of this file is %zu\n",size );
		dprintf(server_fd, "PUT %s\n", remote_name);
		for (int i = 0; i < 8; i++) dprintf(server_fd, "%c", size_arr[i]);
		//fprintf(stderr, "size = %zx\n", size);
		//for (int i = 0; i < 8; i++) fprintf(stderr, "%x\n", size_arr[i]);
		/*int scan = 0;
		char temp;
		while ((scan = fscanf(loc_file, "%c", &temp) != EOF)) {
			dprintf(server_fd, "%c", temp);
		}*/
		char send_file[10000]; memset(send_file, 0, 10000);
		if (size < 9990) {
			fread(send_file, 1, size, loc_file); 
			write(server_fd, send_file, size);
		} else {
			while ((size = fread(send_file, 1, 9900, loc_file))) {
				//fprintf(stderr, "size is %zu\n", size);
				write(server_fd, send_file, size);
				memset(send_file, 0, 10000);
			}
		}
		shutdown(server_fd, SHUT_WR);
		int num = 0; 
		char buffer_put[1000]; memset(buffer_put, 0, 1000);
		char* dis = buffer_put;
		//fprintf(stderr, "up\n");
		while ((num = read(server_fd, dis, 1)) > 0) {
			//printf("num is %d\n", num);
			dis = dis + num;
		}
		//fprintf(stderr, "dddup\n");
		if (strstr(buffer_put, "OK") == buffer_put) {
			print_success();
		} else if (strstr(buffer_put, "ERROR") == buffer_put) {
			char* t = strstr(buffer_put, "\n");
    		t = strstr(t + 1, "\n");
    		char tp = *t;
    		*t = 0;
    		print_error_message(buffer_put + 6);
    		*t = tp;
		} else {
			print_invalid_response();
		}
		
		//read(server_fd, buffer_put, 20);
		
     } else if (do_what == DELETE) {
     	char* remote_name = argv[3];
     	//fprintf(stderr, "%s\n",remote_name);
     	dprintf(server_fd, "DELETE %s\n", remote_name);
     	shutdown(server_fd, SHUT_WR);
     	char buffer_put[1000]; memset(buffer_put, 0, 1000);
		char* dis = buffer_put;
		int num = 0;
		while ((num = read(server_fd, dis, 1)) > 0) {
			dis = dis + num;
		}
		if (strstr(buffer_put, "OK") == buffer_put) {
			print_success();
		} else if (strstr(buffer_put, "ERROR") == buffer_put) {
			char* t = strstr(buffer_put, "\n");
    		t = strstr(t + 1, "\n");
    		char tp = *t;
    		*t = 0;
    		print_error_message(buffer_put + 6);
    		*t = tp;
		} else {
			print_invalid_response();
		}
     } else if (do_what == LIST) {
     	dprintf(server_fd, "LIST\n");
     	shutdown(server_fd, SHUT_WR);
     	//char list_buf[1024]; memset(list_buf, 0, 1024);
     	int num = 0;
     	char* dis = large_buffer;
		// fprintf(stderr, "UP RAED\n" );
        while ((num = read(server_fd, dis, 1)) > 0) {
			// fprintf(stderr, "READ IN LP" );
            dis = dis + num;
		}
		//for (int i = 0; i < 10; i++) fprintf(stderr, "%d\n", large_buffer[i]);
       // fprintf(stderr, "%s hhhhhh\n", large_buffer );
     	size_t size = 0;
     	char* first_time = sousa_get_str(large_buffer, &size);
     	if (first_time == NULL) {
     		char* t = strstr(large_buffer, "\n");
    		t = strstr(t + 1, "\n");
    		char tp = *t;
    		*t = 0;
    		print_error_message(large_buffer + 6);
    		*t = tp;
     	} else if (first_time == (char*)-1) {
     		print_invalid_response();
     	} else {
     		write(1, large_buffer + 11, size);
     	}
     }
    shutdown(server_fd, 0);
    exit(0);
}

/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;
    }

    if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}
