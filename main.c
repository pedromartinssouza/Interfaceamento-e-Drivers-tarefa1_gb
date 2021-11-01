#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define ARRAY_MAX_SIZE 100
#define SENSOR_AMOUNT 2

/* BEGIN TYPE DEFINITIONS */
typedef enum {false,true} bool;
typedef struct ad_values_struct {
    int values[ARRAY_MAX_SIZE];
    int size;
} ad_values_t;
typedef struct sensor_thread_data {
    int thread_id;
    ad_values_t *ad_values;
} sensor_thread_data_t;
typedef struct aux_thread_data {
    sensor_thread_data_t thread_data;
    int target_thread;
} aux_thread_data_t;
/* END TYPE DEFINITIONS */

/* BEGIN ARRAYS TO BE USED BY THREAD TYPES: SENSOR RETRIEVAL THREADS, CLEAR AUX THREADS AND MEAN CALC AUX THREADS*/
sensor_thread_data_t sensor_thread_data_array[SENSOR_AMOUNT];
aux_thread_data_t data_to_clear[SENSOR_AMOUNT];
aux_thread_data_t data_to_calc_mean[SENSOR_AMOUNT];
/* END ARRAYS TO BE USED BY THREAD TYPES */

/* BEGIN THREAD ARRAYS */
pthread_t sensor_threads[SENSOR_AMOUNT];
pthread_t clear_threads[SENSOR_AMOUNT];
pthread_t calc_threads[SENSOR_AMOUNT];
/* BEGIN THREAD ARRAYS */

/* BEGIN GLOBAL VARIABLES */
bool running = true;
float value_means[SENSOR_AMOUNT];
int sizes_considered_for_means[SENSOR_AMOUNT];
/* END GLOBAL VARIABLES */
// values_mutex[SENSOR_AMOUNT] is an array of mutex instances to make sure each AD converter can be locked individually
pthread_mutex_t values_mutex[SENSOR_AMOUNT];

/* BEGIN ASYNCHRONOUS FUNCTIONS */
// value_loop corresponds to an asynchronous loop for retrieving AD values for each channel
void *value_loop(void *thread_arg);
// calc_mean calculates the mean value for a given structure's values
void *calc_mean(void *thread_arg);
// clear_values resets the size of a given structure's array
void *clear_values(void *thread_arg);
/* END ASYNCHRONOUS FUNCTIONS */

/* BEGIN SYNCHRONOUS FUNCTIONS */
// get_ad_value simulates a channel of the AD converter
int get_ad_value();
// insert_value inserts a new AD converter reading in the corresponding structure
void insert_value(ad_values_t *ad_values, int value);
/* END SYNCHRONOUS FUNCTIONS */


int main()
{
    // local variables for thread control
    pthread_attr_t attr;
    int t;
    int return_c;

    // variable to store the addresses of ad_data in the main thread
    ad_values_t ad_data[SENSOR_AMOUNT];

    // initialize thread attribute
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // create ad threads
    for(t = 0; t < SENSOR_AMOUNT; t++)
    {
        // initialize mutex for the thread's ad_values structure
        pthread_mutex_init(&values_mutex[t], NULL);

        ad_data[t].size = 0;
        sensor_thread_data_array[t].thread_id = t;
        sensor_thread_data_array[t].ad_values = (ad_data + t);

        return_c = pthread_create(&sensor_threads[t], &attr, value_loop, (void *) &sensor_thread_data_array[t]);
        if(return_c)
        {
            printf("thread creation error %d\n", t);
            exit(-1);
        }
    }

    // keep running until user quits
    while(running)
    {
        int input;
        printf("\n\nInsert your command:\n1 - Show mean of all values stored\n2 - Clear buffer of values\n0 - Quit\n\n>> ");
        scanf("%d", &input);
        switch (input)
        {
            case 0:
                running = false;
                break;

            // create a thread for each mean to be calculated
            case 1:
                for(int i = 0; i < SENSOR_AMOUNT; i++)
                {
                    data_to_calc_mean[i].thread_data.thread_id = i;
                    data_to_calc_mean[i].thread_data.ad_values = (ad_data + i);
                    data_to_calc_mean[i].target_thread = i;
                    return_c = pthread_create(&calc_threads[i], &attr, calc_mean,
                                              (void *) &data_to_calc_mean[i]);
                    if(return_c)
                    {
                        printf("thread creation error %d\n", i);
                        exit(-1);
                    }
                }
                // wait until all means are calculated
                for (int i = 0; i < SENSOR_AMOUNT; i++)
                {
                    pthread_join(calc_threads[i], NULL);
                }
                for (int i = 0; i < SENSOR_AMOUNT; i++)
                {
                    printf("Mean for sensor channel %d is %f (%d samples considered)\n", i+1, value_means[i], sizes_considered_for_means[i]);
                }
                break;

            // create a thread for each buffer to be cleared
            case 2:
                for(int i = 0; i < SENSOR_AMOUNT; i++)
                {
                    data_to_clear[i].thread_data.thread_id = i;
                    data_to_clear[i].thread_data.ad_values = (ad_data + i);
                    data_to_clear[i].target_thread = i;
                    return_c = pthread_create(&clear_threads[i], &attr, clear_values,
                                              (void *) &data_to_clear[i].thread_data.thread_id);
                    if(return_c)
                    {
                        printf("thread creation error %d\n", i);
                        exit(-1);
                    }
                }
                // wait until all buffers are cleared
                for (int i = 0; i < SENSOR_AMOUNT; i++)
                {
                    pthread_join(clear_threads[i], NULL);
                }
                break;

            default:
                break;
        }
    };
    for (t = 0; t < SENSOR_AMOUNT; t++)
    {
        pthread_join(sensor_threads[t], NULL);
    }
    pthread_exit(NULL);
}

void *value_loop(void *thread_arg)
{
    struct sensor_thread_data *ad_thread_data;
    ad_thread_data = (struct sensor_thread_data *) thread_arg;

    int task_id = ad_thread_data->thread_id;
    ad_values_t *task_ad_data = ad_thread_data->ad_values;

    // each thread shall have its own random seed
    srand((int) task_ad_data);

    while(running)
    {
        pthread_mutex_lock(&values_mutex[task_id]);
        insert_value(task_ad_data, get_ad_value());
        pthread_mutex_unlock(&values_mutex[task_id]);
        sleep(1);
    }
    pthread_exit(NULL);
    return NULL;
}

void *calc_mean(void *thread_arg)
{
    aux_thread_data_t *ad_thread_data;
    ad_thread_data = (aux_thread_data_t *) thread_arg;

    int target_thread = ad_thread_data->target_thread;
    ad_values_t ad_values_copy = *ad_thread_data->thread_data.ad_values;

    float total = 0;
    for (int i = 0; i < ad_values_copy.size; i++)
    {
        total = total + (float) ad_values_copy.values[i];
    }
    value_means[target_thread] = total/(float) ad_values_copy.size;
    sizes_considered_for_means[target_thread] = ad_values_copy.size;

    pthread_exit(NULL);
    return NULL;
}

void *clear_values(void *thread_arg)
{
    aux_thread_data_t *ad_thread_data;
    ad_thread_data = (aux_thread_data_t *) thread_arg;

    int target_thread = ad_thread_data->target_thread;
    ad_values_t *task_ad_data = ad_thread_data->thread_data.ad_values;

    pthread_mutex_lock(&values_mutex[target_thread]);
    task_ad_data->size = 0;
    pthread_mutex_unlock(&values_mutex[target_thread]);
    pthread_exit(NULL);
    return NULL;
}

int get_ad_value()
{
    return rand() % 20;
}

void insert_value(ad_values_t *ad_values, int value)
{
    // shift numbers 1 position to the left
    if(ad_values->size == ARRAY_MAX_SIZE)
    {
        for (int i = 0; i < ad_values->size - 1; i++)
        {
            ad_values->values[i] = ad_values->values[i+1];
        }
        ad_values->size--;
    }

    ad_values->values[ad_values->size] = value;

    ad_values->size++;
}