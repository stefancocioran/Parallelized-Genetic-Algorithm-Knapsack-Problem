#include "genetic_algorithm_par.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_input(sack_object **objects, int *object_count, int *sack_capacity,
               int *generations_count, int *threads, int argc, char *argv[]) {
    FILE *fp;

    if (argc < 4) {
        fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count P\n");
        return 0;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        return 0;
    }

    if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
        fclose(fp);
        return 0;
    }

    if (*object_count % 10) {
        fclose(fp);
        return 0;
    }

    sack_object *tmp_objects =
        (sack_object *)calloc(*object_count, sizeof(sack_object));

    for (int i = 0; i < *object_count; ++i) {
        if (fscanf(fp, "%d %d", &tmp_objects[i].profit,
                   &tmp_objects[i].weight) < 2) {
            free(objects);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);

    *generations_count = (int)strtol(argv[2], NULL, 10);

    *threads = (int)strtol(argv[3], NULL, 10);

    if (*generations_count == 0) {
        free(tmp_objects);

        return 0;
    }

    *objects = tmp_objects;

    return 1;
}

void print_objects(const sack_object *objects, int object_count) {
    for (int i = 0; i < object_count; ++i) {
        printf("%d %d\n", objects[i].weight, objects[i].profit);
    }
}

void print_generation(const individual *generation, int limit) {
    for (int i = 0; i < limit; ++i) {
        for (int j = 0; j < generation[i].chromosome_length; ++j) {
            printf("%d ", generation[i].chromosomes[j]);
        }

        printf("\n%d - %d\n", i, generation[i].fitness);
    }
}

void print_best_fitness(const individual *generation) {
    printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects,
                              individual *generation, int object_count,
                              int sack_capacity, int thread_id, int P) {
    int weight;
    int profit;
    int start = thread_id * ceil((double)object_count / P);
    int end =
        MIN((thread_id + 1) * ceil((double)object_count / P), object_count);

    // number of iterations for computing the fitness is divided equally between
    // threads
    for (int i = start; i < end; ++i) {
        weight = 0;
        profit = 0;
        int chormosomes_count = 0;
        for (int j = 0; j < generation[i].chromosome_length; ++j) {
            if (generation[i].chromosomes[j]) {
                weight += objects[j].weight;
                profit += objects[j].profit;
            }
            chormosomes_count += generation[i].chromosomes[j];
        }
        generation[i].chromosome_nr = chormosomes_count;
        generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
    }
}

int cmpfunc(const void *a, const void *b) {
    individual *first = (individual *)a;
    individual *second = (individual *)b;

    int res = second->fitness - first->fitness;  // decreasing by fitness
    if (res == 0) {
        int first_count = first->chromosome_nr,
            second_count = second->chromosome_nr;

        // since this "for" loop is the same as the one from
        // "compute_fitness_function" the operations that should have been done
        // here were moved there and the "count" variabiles were stored in a new
        // field of the "individual" structure

        /* for (int i = 0;
             i < first->chromosome_length && i < second->chromosome_length;
             ++i) {
            first_count += first->chromosomes[i];
            second_count += second->chromosomes[i];
        } */

        res = first_count -
              second_count;  // increasing by number of objects in the sack
        if (res == 0) {
            return second->index - first->index;
        }
    }

    return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index) {
    int i, mutation_size;
    int step = 1 + generation_index % (ind->chromosome_length - 2);

    if (ind->index % 2 == 0) {
        // for even-indexed individuals, mutate the first 40% chromosomes by a
        // given step
        mutation_size = ind->chromosome_length * 4 / 10;
        for (i = 0; i < mutation_size; i += step) {
            ind->chromosomes[i] = 1 - ind->chromosomes[i];
        }
    } else {
        // for even-indexed individuals, mutate the last 80% chromosomes by a
        // given step
        mutation_size = ind->chromosome_length * 8 / 10;
        for (i = ind->chromosome_length - mutation_size;
             i < ind->chromosome_length; i += step) {
            ind->chromosomes[i] = 1 - ind->chromosomes[i];
        }
    }
}

void mutate_bit_string_2(const individual *ind, int generation_index) {
    int step = 1 + generation_index % (ind->chromosome_length - 2);

    // mutate all chromosomes by a given step
    for (int i = 0; i < ind->chromosome_length; i += step) {
        ind->chromosomes[i] = 1 - ind->chromosomes[i];
    }
}

void crossover(individual *parent1, individual *child1, int generation_index) {
    individual *parent2 = parent1 + 1;
    individual *child2 = child1 + 1;
    int count = 1 + generation_index % parent1->chromosome_length;

    memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
    memcpy(child1->chromosomes + count, parent2->chromosomes + count,
           (parent1->chromosome_length - count) * sizeof(int));

    memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
    memcpy(child2->chromosomes + count, parent1->chromosomes + count,
           (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to) {
    memcpy(to->chromosomes, from->chromosomes,
           from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation) {
    int i;

    for (i = 0; i < generation->chromosome_length; ++i) {
        free(generation[i].chromosomes);
        generation[i].chromosomes = NULL;
        generation[i].fitness = 0;
    }
}

void *run_genetic_algorithm(void *arg) {
    int count, cursor;
    struct my_arg data = *(struct my_arg *)arg;
    int start = data.thread_id * ceil((double)data.object_count / data.P);
    int end =
        MIN((data.thread_id + 1) * ceil((double)data.object_count / data.P),
            data.object_count);
    individual *tmp = NULL;

    // set initial generation (composed of object_count individuals with a
    // single item in the sack)
    for (int i = start; i < end; ++i) {
        data.current_generation[i].fitness = 0;
        data.current_generation[i].chromosomes[i] = 1;
        data.current_generation[i].index = i;
        data.current_generation[i].chromosome_length = data.object_count;

        data.next_generation[i].fitness = 0;
        data.next_generation[i].index = i;
        data.next_generation[i].chromosome_length = data.object_count;
    }

    // ensures that everything is set correctly before starting the actual
    // execution of the algorithm
    pthread_barrier_wait(data.barrier);

    // iterate for each generation
    for (int k = 0; k < data.generations_count; ++k) {
        cursor = 0;
        // compute fitness and sort by it
        compute_fitness_function(data.objects, data.current_generation,
                                 data.object_count, data.sack_capacity,
                                 data.thread_id, data.P);
        // wait for all threads to finish computing the fitness to have a
        // correct value
        pthread_barrier_wait(data.barrier);

        // the sort should only be done by one thread because
        // a race condition could occur and the result might not be correct
        if (data.thread_id == 0) {
            qsort(data.current_generation, data.object_count,
                  sizeof(individual), cmpfunc);
        }

        // all threads must wait for the sort to be completed
        pthread_barrier_wait(data.barrier);

        // keep first 30% children (elite children selection)
        count = data.object_count * 3 / 10;
        start = data.thread_id * ceil((double)count / data.P);
        end = MIN((data.thread_id + 1) * ceil((double)count / data.P), count);
        // parallelized the copying of the individuals
        for (int i = start; i < end; ++i) {
            copy_individual(data.current_generation + i,
                            data.next_generation + i);
        }

        // ensures that the copying is finished before continuing
        pthread_barrier_wait(data.barrier);

        cursor = count;

        // mutate first 20% children with the first version of bit string
        // mutation
        count = data.object_count * 2 / 10;
        start = data.thread_id * ceil((double)count / data.P);
        end = MIN((data.thread_id + 1) * ceil((double)count / data.P), count);

        for (int i = start; i < end; ++i) {
            copy_individual(data.current_generation + i,
                            data.next_generation + cursor + i);
            mutate_bit_string_1(data.next_generation + cursor + i, k);
        }

        // ensures that the mutation occured correctly
        pthread_barrier_wait(data.barrier);

        cursor += count;

        // mutate next 20% children with the second version of bit string
        // mutation
        count = data.object_count * 2 / 10;
        start = data.thread_id * ceil((double)count / data.P);
        end = MIN((data.thread_id + 1) * ceil((double)count / data.P), count);

        for (int i = start; i < end; ++i) {
            copy_individual(data.current_generation + i + count,
                            data.next_generation + cursor + i);
            mutate_bit_string_2(data.next_generation + cursor + i, k);
        }

        pthread_barrier_wait(data.barrier);

        cursor += count;

        // crossover first 30% parents with one-point crossover
        // (if there is an odd number of parents, the last one is kept as such)
        count = data.object_count * 3 / 10;

        // this should inly be done by one thread because data might turn out
        // altered
        if (count % 2 == 1 && data.thread_id == 0) {
            copy_individual(data.current_generation + data.object_count - 1,
                            data.next_generation + cursor + count - 1);
        }

        pthread_barrier_wait(data.barrier);

        count--;

        if (data.thread_id == 0) {
            for (int i = 0; i < count; i += 2) {
                crossover(data.current_generation + i,
                          data.next_generation + cursor + i, k);
            }
        }

        pthread_barrier_wait(data.barrier);

        // switch to new generation
        tmp = data.current_generation;
        data.current_generation = data.next_generation;
        data.next_generation = tmp;

        // in order to continue correctly, all threads should be up to date
        pthread_barrier_wait(data.barrier);

        start = data.thread_id * ceil((double)data.object_count / data.P);
        end =
            MIN((data.thread_id + 1) * ceil((double)data.object_count / data.P),
                data.object_count);

        for (int i = start; i < end; ++i) {
            data.current_generation[i].index = i;
        }

        // only one thread prints the fitness
        if (k % 5 == 0 && data.thread_id == 0) {
            print_best_fitness(data.current_generation);
        }
    }

    compute_fitness_function(data.objects, data.current_generation,
                             data.object_count, data.sack_capacity,
                             data.thread_id, data.P);
    pthread_barrier_wait(data.barrier);

    // the final sort and the print is done by one thread
    if (data.thread_id == 0) {
        qsort(data.current_generation, data.object_count, sizeof(individual),
              cmpfunc);
        print_best_fitness(data.current_generation);
    }

    pthread_exit(NULL);
}