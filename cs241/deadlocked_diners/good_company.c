/**
 * Deadlocked Diners Lab
 * CS 241 - Fall 2017
 */
#include "company.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *work_interns(void *p) {
    Company *company = (Company *)p;
	pthread_mutex_t *left_intern, *right_intern;
	left_intern = Company_get_left_intern(company);
    right_intern = Company_get_right_intern(company);
	
	pthread_mutex_t* first_monkey = left_intern < right_intern ? left_intern : right_intern;
	pthread_mutex_t* second_monkey = left_intern == first_monkey ? right_intern : left_intern;
	
	while (running && first_monkey == second_monkey) Company_have_board_meeting(company);
	
	while (running) {
		int first_monkey_grab_failed = pthread_mutex_trylock(first_monkey);
		//if first_monkey_grab == 0, successfully grab first_monkey
		if (!first_monkey_grab_failed) {
			int second_monkey_grab_failed = pthread_mutex_trylock(second_monkey);
			if (!second_monkey_grab_failed) {
				Company_hire_interns(company);
    		    pthread_mutex_unlock(first_monkey);
    		    pthread_mutex_unlock(second_monkey);
    		    Company_have_board_meeting(company);
			} else {
				pthread_mutex_unlock(first_monkey);
			}
		}
		
	
	}
	
	    
    return NULL;
}
