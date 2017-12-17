/**
 * Parallel Make
 * CS 241 - Fall 2017
 */


#include "format.h"
#include "graph.h"
#include "parmake.h"
#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>
#include <pthread.h>
#include "set.h"
#include "dictionary.h"
#include "compare.h"
#include <time.h>
#include "queue.h"
#include <sys/time.h>

//int qsize;
typedef struct _data {
	queue* q;
	set* q_contain;
	set* need_to_do;
} data;


graph* g;


pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

int run_file(graph* g, char* it);
int run_cmd(graph* g, char* it);
int run_sys_cmds_old(vector* cmds, rule_t* rule);
	
int is_file(char* fname) {
    FILE *file;
    if ((file = fopen(fname, "r"))) {
        fclose(file);
        return 1;
    }
    return 0;
}

void DFS_detect(graph* g, char* now_v, dictionary* par, set* repeats, set* visited) {
	set_add(visited, now_v);
	vector* this_nei = graph_neighbors(g, now_v);
	size_t nei_size = vector_size(this_nei);
	for (size_t i = 0; i < nei_size; i++) {
		char* this_v = vector_get(this_nei, i);
		if (set_contains(visited, this_v)) {
			set_add(repeats, now_v);
			//printf("CYCLE! str is %s\n", now_v);
			continue;
		}
		dictionary_set(par, this_v, now_v);
		DFS_detect(g, this_v, par, repeats, visited);
		set_remove(visited, this_v);	
	}
	
	vector_destroy(this_nei);
} 

void dependency_baba(graph* g, char* it, dictionary* par, set* cycle) {
	if (strcmp(it, "") == 0) return;
	//printf("str is %s\n",it);
	set_add(cycle, it);
	if (graph_contains_vertex(g, it)) {
		graph_remove_vertex(g, it); //删掉节点
		//if (strcmp(it, "") != 0) print_cycle_failure((char*)it);
		//rule_t* rule = (rule_t *)graph_get_vertex_value(g, it);
		//rule->state = -66; // -66 means cycle dependency
	}
	dependency_baba(g, dictionary_get(par, it), par, cycle);	
}

int run_sys_cmds(rule_t* rule) {
	vector* cmds = rule->commands;
	size_t cmd_size = vector_size(cmds);
	for (size_t i = 0; i < cmd_size; i++) {
		char* command = vector_get(cmds, i);
		int sys_ret = system(command);
		if (sys_ret != 0) {
			return -1;
		}	
	}
	return 1; 
}

void push_all_leaf(graph* g, queue* q, char* it, set* used, set* need_to_do, set* q_contain) {
	if (it == NULL) return;
	set_add(need_to_do, it);
	vector* my_neighbors = graph_neighbors(g, it);
	if (vector_size(my_neighbors) == 0 && !set_contains(used, it)) {
		set_add(used, it);
		//qsize++;
		queue_push(q, it);
		set_add(q_contain, it);
	}
	for (size_t i = 0; i < vector_size(my_neighbors); i++) {
		push_all_leaf(g, q, vector_get(my_neighbors, i), used, need_to_do, q_contain);
	}
	vector_destroy(my_neighbors);
}

void* work(void* arg) {
	data* shuju = (data*)arg;
	queue* q = shuju->q;
	set* q_contain = shuju->q_contain;
	set* need_to_do = shuju->need_to_do;
	vector* pars;
RESTART:
	while (1) {
	
	
	
		//fprintf(stderr, "pull\n");
		char* node = queue_pull(q);
		//fprintf(stderr, "pull down, node is %s\n",node);
		//fprintf(stderr, "str pulled is %s\n",node);
		
		
		if (strcmp(node, "") == 0) {
			//fprintf(stderr, "hahah,finsih\n");
			queue_push(q, node);
			queue_push(q, node);
			queue_push(q, node);
			return NULL;
		}
		if (!set_contains(need_to_do, node)) {
			//fprintf(stderr, "str unused is %s\n",node);
			continue;
		}
		
		rule_t* my_rule = (rule_t *)graph_get_vertex_value(g, node);
		vector* my_nei = graph_neighbors(g, node);

		size_t vec_size = vector_size(my_nei);
		if (my_rule->state == 1 || my_rule->state == -1) {
			goto PAR_GET;
		}
		if (vec_size == 0) {
			//fprintf(stderr, "leaf str pulled is %s\n",node);
			// I am a leaf
			//fprintf(stderr, "in leaf, str is %s\n", node);
			int sys_res = run_sys_cmds(my_rule);
			pthread_mutex_lock(&m);
			if (sys_res > 0) my_rule->state = 1;
			else my_rule->state = -1;
			pthread_mutex_unlock(&m);
			//fprintf(stderr, "finish leaf, str is %s\n", node);
		} else {
			int run_this_node = 1;
			if (is_file(node)) {
				// I am a file
				
				//fprintf(stderr, "I am file, node is %s\n", node);
				for (size_t i = 0; i < vec_size; i++) {
					char* this_v = vector_get(my_nei, i);
					rule_t* this_rule = (rule_t *)graph_get_vertex_value(g, this_v);	
					pthread_mutex_lock(&m);
						if (this_rule->state == 0) {
							queue_push(q, node);
							pthread_mutex_unlock(&m);
							goto RESTART;
						}
						
						
						while (this_rule->state == 0) {
							//printf("SLEEP this node is %s, this rule state is %d\n", this_v, this_rule->state);
							pthread_cond_wait(&cv, &m);
						}
					pthread_mutex_unlock(&m);
					
					if (is_file(this_v)) {
						struct stat file_stat_this_v;
						struct stat file_stat_node;
						stat(this_v, &file_stat_this_v);
						stat(node, &file_stat_node);
						/*if (file_stat_this_v.st_mtime < file_stat_node.st_mtime) {
							run_this_node = 0;
							continue;
						}*/
						/*if (this_rule->state == -1) {
							run_this_node = 0;
							pthread_mutex_lock(&m);
							my_rule->state = -1;
							pthread_mutex_unlock(&m);
							break;
						}*/
						
						if (difftime(file_stat_node.st_mtime, file_stat_this_v.st_mtime) > 0) {
							// not depend on a newer file
							run_this_node = 0;
							pthread_mutex_lock(&m);
							my_rule->state = 1;
							pthread_mutex_unlock(&m);
							continue;
						} else {
						}
						
					} else {
						if (this_rule->state == 1) {
							continue;
						} else if (this_rule->state == -1) {
							//fprintf(stderr, "one this v fail, %s\n",this_v);
							run_this_node = 0;
							pthread_mutex_lock(&m);
							my_rule->state = -1;
							pthread_mutex_unlock(&m);
							break;
						} else {
						}
					}
				}
			} else {
				//I am not a file
				for (size_t i = 0; i < vec_size; i++) {
					char* this_v = vector_get(my_nei, i);
					rule_t* this_rule = (rule_t *)graph_get_vertex_value(g, this_v);
					
					//printf("this node is %s, this rule state is %d\n", this_v, this_rule->state);
					pthread_mutex_lock(&m);
					if (this_rule->state == 0) {
							queue_push(q, node);
							pthread_mutex_unlock(&m);
							goto RESTART;
					}
					while (this_rule->state == 0) {
						//printf("SLEEP this node is %s, this rule state is %d\n", this_v, this_rule->state);
						pthread_cond_wait(&cv, &m);
					}
					pthread_mutex_unlock(&m);	
					if (this_rule->state == 1) {
						continue;
					} else if (this_rule->state == -1) {
						run_this_node = 0;
						//pthread_mutex_lock(&m);
						my_rule->state = -1;
						//pthread_mutex_unlock(&m);
						break;
					}
				}
			}

			if (run_this_node) {
				int res = run_sys_cmds(my_rule);
				
				if (res > 0) {
					my_rule->state = 1;
				} else {
					my_rule->state = -1;
				}
			} else {
				//my_rule->state = -1;
			}
			
		}
PAR_GET:
		pars = graph_antineighbors(g, node);
			
		for (size_t i = 0; i < vector_size(pars); i++) {
			//qsize++;
			char* t = vector_get(pars, i);
			//fprintf(stderr, "NOW LP STR is %s\n", t);
			//pthread_mutex_lock(&m);
			int q_has_t = set_contains(q_contain, t);
			if (!q_has_t) set_add(q_contain, t);
			//pthread_mutex_unlock(&m);
			if (!q_has_t) queue_push(q, t);
		}
			
		pthread_cond_broadcast(&cv);
		vector_destroy(pars);
		vector_destroy(my_nei);
	}
	return NULL;
} 

void target_make(size_t num_threads, char* target) {
	set* used = string_set_create();
	queue* q = queue_create(-1);
	set* q_contain = string_set_create();
	set* need_to_do = string_set_create();
	push_all_leaf(g, q, target, used, need_to_do, q_contain);
	data shuju;
	shuju.q = q;
	shuju.q_contain = q_contain;
	shuju.need_to_do = need_to_do;
	
	pthread_t threads[10000];
	//num_threads = num_threads > 5 ? num_threads : 5;
	for (size_t i = 0; i < num_threads; i++) {
		pthread_create(&threads[i],NULL,&work, (void*)&shuju);
	}
	
	for (size_t i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}
	
	queue_destroy(q);
	set_destroy(used);
	set_destroy(need_to_do);
	set_destroy(q_contain);
}



int parmake(char *makefile, size_t num_threads, char **targets) {
	dictionary* par = string_to_string_dictionary_create(); // clear
	set* visited  = string_set_create(); //clear
    set* repeats  = string_set_create(); //clear
    set* cycle = string_set_create(); //clear
    g = parser_parse_makefile(makefile, targets); // 

    vector* target_names_begin = graph_vertices(g); // clear
	char* first_node = vector_get(target_names_begin, 0);
	//dictionary_set(par, first_node, "NO_PAR");
	char start_str[1000]; memset(start_str, 0, 1000);
	strcpy(start_str, vector_get(target_names_begin, 1));
// void DFS_detect(graph* g, char* now_v, dictionary* par, set* repeats, set* visited) {
	DFS_detect(g, first_node, par, repeats, visited);
	
	SET_FOR_EACH(repeats, it, {
		dependency_baba(g, it, par, cycle); // put all cycle nodes in cycle set and remove the cycle nodes
	});

	set_destroy(visited);
	set_destroy(repeats);
	dictionary_destroy(par);

	if (targets[0] == NULL) {
		//printf("now str is %s\n", start_str)
		if (set_contains(cycle, start_str)) {
			print_cycle_failure(start_str);
		} else {
		}
	} else {
		for (char** it = targets; *it != NULL; it++) {
			if (set_contains(cycle, *it)) {
				print_cycle_failure(*it);
			}
		}
	}
	
	vector_destroy(target_names_begin);
	//finish detect cycle
	
    vector* nodes = graph_vertices(g);

    if (num_threads == 1) {
	    //char* start_str = vector_get(nodes, 1);
	    if (targets[0] == NULL) {
	    	if (set_contains(cycle,start_str)) goto ERR;
	    	if (is_file(start_str)) {
				run_file(g, start_str);
			} else {
				run_cmd(g, start_str);
			}   
	    } else {
			for (char** it = targets; *it != NULL; it++) {
				if (set_contains(cycle,*it)) continue;
				if (is_file(*it)) {
					run_file(g, *it);
				} else {
					run_cmd(g, *it);
				}   
				
			}
		}	
ERR:	set_destroy(cycle);
	    vector_destroy(nodes);
		//queue_destroy(q);
		graph_destroy(g);
		pthread_cond_destroy(&cv);
		pthread_mutex_destroy(&m);
    	return 0;
	}
/*	
	set* used = string_set_create();
	queue* q = queue_create(-1);
	set* q_contain = string_set_create();
	set* need_to_do = string_set_create();
	push_all_leaf(g, q, target, used, need_to_do, q_contain);
	data shuju;
	shuju.q = q;
	shuju.q_contain = q_contain;
	shuju.need_to_do = need_to_do;
	
	pthread_t threads[10000];
	//num_threads = num_threads > 5 ? num_threads : 5;
	for (size_t i = 0; i < num_threads; i++) {
		pthread_create(&threads[i],NULL,&work, (void*)&shuju);
	}
	
	for (size_t i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}
	
	queue_destroy(q);
	set_destroy(used);
	set_destroy(need_to_do);
	set_destroy(q_contain);	
	
*/	
	if (targets[0] == NULL) {
		//printf("now str is %s\n", start_str)
		target_make(num_threads, vector_get(nodes, 0));
	} else {
		set* used = string_set_create();
		queue* q = queue_create(-1);
		set* need_to_do = string_set_create();
		set* q_contain = string_set_create();
		data shuju;
		shuju.q = q;
		shuju.q_contain = q_contain;
		shuju.need_to_do = need_to_do;
		pthread_t threads[10000];
		for (char** it = targets; *it != NULL; it++) {
			if (set_contains(cycle, *it)) continue;
			push_all_leaf(g, q, *it, used, need_to_do, q_contain);
			//target_make(num_threads, *it);
			//fprintf(stderr, "FINSIH it %s\n", *it);
		}
		for (size_t i = 0; i < num_threads; i++) {
			pthread_create(&threads[i],NULL,&work, (void*)&shuju);
		}	
	
		for (size_t i = 0; i < num_threads; i++) {
			pthread_join(threads[i], NULL);
		}
		queue_destroy(q);
		set_destroy(used);
		set_destroy(need_to_do);
		set_destroy(q_contain);	
		
	}
	set_destroy(cycle);

    vector_destroy(nodes);
	//queue_destroy(q);
	graph_destroy(g);
	//set_destroy(q_contain);
	pthread_cond_destroy(&cv);
	pthread_mutex_destroy(&m);
	return 0;
}


int run_file(graph* g, char* it) {
	rule_t* rule = (rule_t *)graph_get_vertex_value(g, it);
	int retval = 1;

	int file_run_flag = 1;
	vector* this_nei = graph_neighbors(g, it);
	size_t vec_size = vector_size(this_nei);
	if (vec_size == 0) {
		// leaf file
		vector* cmds = rule->commands;
		retval = run_sys_cmds_old(cmds, rule);
		rule->state = retval;
	} else {
		// not a leaf file, run dependencies
		for (size_t i = 0; i < vec_size; i++) {
			char* this_v = vector_get(this_nei, i);
			if (is_file(this_v)) {
				struct stat file_stat_this_v;
				struct stat file_stat_it;
				stat(this_v, &file_stat_this_v);
				stat(it, &file_stat_it);
				// if depend < father, continue, not run
				if (difftime(file_stat_it.st_mtime, file_stat_this_v.st_mtime) > 0)  {
					file_run_flag = 0;
					rule->state = 1;
					continue;
				} else {
					int rf = run_file(g, this_v);
					if (rf < 0) retval = rf;
				}
			} else {
				rule_t* rule_v = (rule_t *)graph_get_vertex_value(g, this_v);
				if (rule_v->state > 0) continue;
				int rc = run_cmd(g, this_v);
				if (rc < 0) retval = rc;
				
			}
		}
		if (retval > 0 && file_run_flag) retval = run_sys_cmds_old(rule->commands, rule);
	} 
	vector_destroy(this_nei);
	return retval;
}

int run_cmd(graph* g, char* it) {
	int retval = 1;
	rule_t* rule = (rule_t *)graph_get_vertex_value(g, it);
	//printf("rule state is %d\n", rule->state);
	int file_run_flag = 1;
	vector* this_nei = graph_neighbors(g, it);
	size_t vec_size = vector_size(this_nei);	
	if (vec_size == 0) {
		//leaf cmd
		vector* cmds = rule->commands;
		retval = run_sys_cmds_old(cmds, rule);
		rule->state = retval;
	} else {
		// not a leaf cmd, run dependencies
		for (size_t i = 0; i < vec_size; i++) {
			char* this_v = vector_get(this_nei, i);
			if (is_file(this_v)) {
				int rf = run_file(g, this_v);
				if (rf < 0) {
					file_run_flag = 0;
					rule->state = -1;
					break;
				} else {
				}

			} else {
				rule_t* rule_v = (rule_t *)graph_get_vertex_value(g, this_v);
				if (rule_v->state > 0) continue;
				int rc = run_cmd(g, this_v);
				if (rc < 0) retval = rc;
				
			}
		}
		if (retval > 0 && file_run_flag) retval = run_sys_cmds_old(rule->commands, rule);
	}
	vector_destroy(this_nei);
	return retval;
}

int run_sys_cmds_old(vector* cmds, rule_t* rule) {
	size_t cmd_size = vector_size(cmds);
	for (size_t i = 0; i < cmd_size; i++) {
		char* command = vector_get(cmds, i);
		int sys_ret = system(command);
		if (sys_ret != 0) {
			rule->state = -1;
			return -1;
		}	
	}
	return 1;
}
