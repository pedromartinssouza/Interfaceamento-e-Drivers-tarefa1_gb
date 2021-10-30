#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_SIZE 100
#define SENSOR_AMOUNT 2

typedef struct ad_values_struct {
    int values[ARRAY_SIZE][SENSOR_AMOUNT];
    int next_occupied_position;
} ad_values_t;
typedef enum { false, true } bool;

ad_values_t ad_values_array;
bool running = true;
pthread_mutex_t values_mutex;

void insert_value(const int *values);
void calc_mean();
void *value_loop(void *thread_id);
void *clear_values(void *thread_id);

int main()
{

    pthread_t threads[3];
    pthread_attr_t attr;
    int t;
    int return_c;

    pthread_mutex_init(&values_mutex, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    ad_values_array.next_occupied_position = 0;
    return_c = pthread_create(&threads[0], &attr, value_loop, (void *) t);
    if(return_c)
    {
        printf("thread creation error %d\n", t);
        exit(-1);
    }

    while(running)
    {
        int input;
        printf("Insert your command:\n1 - Show mean of all values stored\n2 - Clear buffer of values\n0 - Quit\n\n>> ");
        scanf("%d", &input);
        switch (input)
        {
            case 1:
                calc_mean();
                break;
            case 2:
                return_c = pthread_create(&threads[1], &attr, clear_values, (void *)t);
                if(return_c)
                {
                    printf("thread creation error %d\n", t);
                    exit(-1);
                } else {
                    pthread_join(threads[1], NULL);
                    printf("\nBuffer cleared\n\n");
                }
                break;
            case 0:
                running = false;
                break;
            default:
                break;
        }
    };
    pthread_join(threads[0], NULL);
    pthread_exit(NULL);
}

void *value_loop(void *thread_id)
{
    srand(time(NULL));
    while(running)
    {
        //TODO: insert random numbers
        int nums[2];
        nums[0] = rand() % 20;
        nums[1] = rand() % 20;
        pthread_mutex_lock(&values_mutex);
        insert_value(nums);
        pthread_mutex_unlock(&values_mutex);
        sleep(1);
    }
    pthread_exit(NULL);
}

void *clear_values(void *thread_id)
{
    pthread_mutex_lock(&values_mutex);
    ad_values_array.next_occupied_position = 0;
    pthread_mutex_unlock(&values_mutex);
}

void calc_mean()
{
    ad_values_t ad_values_copy = ad_values_array;

    float total_1 = (float) ad_values_copy.values[0][0];
    float total_2 = (float) ad_values_copy.values[0][1];
    for (int i = 1; i < ad_values_copy.next_occupied_position; i++)
    {
        total_1 = (float) (total_1 + ad_values_copy.values[i][0])/2.0;
        total_2 = (float) (total_2 + ad_values_copy.values[i][1])/2.0;
    }
    printf("\nThe mean for %d registers is %f and %f\n\n", ad_values_copy.next_occupied_position, total_1, total_2);
}

void insert_value(const int *values)
{

    if(ad_values_array.next_occupied_position == ARRAY_SIZE)
    {
        for (int i = 0; i < ad_values_array.next_occupied_position - 1; i++)
        {
            for (int j = 0; j < SENSOR_AMOUNT; j++){
                ad_values_array.values[i][j] = ad_values_array.values[i+1][j];
            }
        }
        ad_values_array.next_occupied_position--;
    }


    for (int j = 0; j < SENSOR_AMOUNT; j++){
        ad_values_array.values[ad_values_array.next_occupied_position][j] = *(values + j);
    }

    ad_values_array.next_occupied_position++;
}