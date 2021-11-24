#include <stdlib.h>
#include "genetic_algorithm_par.h"
#include <stdio.h>


int main(int argc, char *argv[]) {
	// array with all the objects that can be placed in the sack
	sack_object *objects = NULL;

	// number of objects
	int object_count = 0;

	// maximum weight that can be carried in the sack
	int sack_capacity = 0;

	// number of generations
	int generations_count = 0;

	// number of threads
	int P = 0;

	void *status;
	pthread_t threads[P];
	pthread_barrier_t barrier;

	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, &P, argc, argv)) {
		return 0;
	}

	pthread_barrier_init(&barrier, NULL, P);

	int r;
	struct my_arg arguments[P]; 

	// memory for initial generation was allocated here because calloc/malloc is not thread-safe 
	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));

	for (int i = 0; i < object_count; ++i) {
		current_generation[i].chromosomes = (int*) calloc(object_count, sizeof(int));
		next_generation[i].chromosomes = (int*) calloc(object_count, sizeof(int));
	}

	// thread function arguments were passed as a struct
	for (int i = 0; i < P; ++i) {
		arguments[i].current_generation = current_generation;
		arguments[i].next_generation = next_generation;
		arguments[i].generations_count = generations_count;
		arguments[i].object_count = object_count;
		arguments[i].objects = objects;
		arguments[i].barrier = &barrier;
		arguments[i].thread_id = i;
		arguments[i].P = P;
		arguments[i].sack_capacity = sack_capacity;
	
		r = pthread_create(&threads[i], NULL, run_genetic_algorithm, &arguments[i]);

		if (r) {
			printf("Eroare la crearea thread-ului %d\n", i);
			exit(-1);
		}
	}

	for (int i = 0; i < P; i++) {
		r = pthread_join(threads[i], &status);

		if (r) {
			printf("Eroare la asteptarea thread-ului %d\n", i);
			exit(-1);
		}
	}

	if (pthread_barrier_destroy(&barrier) != 0) {
		printf("Eroare la dezalocarea barierei\n");
		exit(-1);
	}

	// free resources for old generation
	free_generation(current_generation);
	free_generation(next_generation);

	// free resources
	free(objects);
	free(current_generation);
	free(next_generation);

	return 0;
}
