/*
Name: Michelle Lin 
netid: ml7188
CITATIONS:
1. Thread Pools in C: https://www.youtube.com/watch?v=_n2hE2gyPxU
I used this video to learn how to implement a thread pool, including
submitting tasks and get tasks
*/


//todo: 好多好多好多segmentation fault
//把所有报错信息改成stderr
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>

pthread_mutex_t mutex;
pthread_mutex_t queue_mutex;
pthread_cond_t cond_queue;
pthread_mutex_t result_mutex;

size_t standard = 4096;

// u_int8_t **threading_count_result;
// char **threading_character_result;
int *threading_result_length;
char **threading_result;

// nyuenc: when nyuenc is run, 
// it would take the arguments in argv 
// if only one file: encode it 
// if multiple files: concatenate and encode it

//main thread creates task and insert it to the task queue
//threads in thread pool wait and get a task, and execute the task
//send a result to the main thread

//main function: map files
//submit tasks by files, each with 4kb chunks of memory, call execute with 4kb chunks of memory
//接threads, put thread outputs into an array
//go through the array and stitch stuff together and print output


//如何接threads：create一个巨大的数组，然后每个task有对应的号码，放到对应的地方去就好了。然后拿main function遍历一遍 
//main function encode完成之后要会自己停下。确认好所有task都submit了然后所有task都做完了要停下

struct Task 
{
    int start;
    int end;
    int task_number;
    char * file_mem;
};

struct Task task_queue[1000000];
int taskcount = 0;
int front = 0;
int back = 0;

void *execute_encoder(struct Task task_to_execute)
{   

    int j;
    int a = 0, b = 1;
    u_int8_t count; 

    for (j = task_to_execute.start; j < task_to_execute.end; j++)
    {
        int next = j+1;
        u_int8_t count = 1; 
        
        while (task_to_execute.file_mem[j] == task_to_execute.file_mem[next] && next < task_to_execute.end)
        {
            count++;
        
            next++;
            if(!(next < task_to_execute.end))
            {
                break;
            }

        }
        
        threading_result[task_to_execute.task_number][a] = task_to_execute.file_mem[j];
        // printf("threading_result[%d][%d] = %c\n",task_to_execute.task_number, a, threading_result[task_to_execute.task_number][a]);
        threading_result[task_to_execute.task_number][b] = count;
        // printf("threading_result[%d][%d] = %d\n",task_to_execute.task_number, b, count);
        
        a += 2;
        b += 2;
        j = next-1;
    }
    // printf("a = %d", a);
    threading_result[task_to_execute.task_number][a] = NULL; //put null at the very back
    // threading_result_length[task_to_execute.task_number] = a-2;
    int i;
    for (i = 0; threading_result[task_to_execute.task_number][i] != NULL; i++)
    {
        ;
    }
    // printf("i = %d", i);
    threading_result_length[task_to_execute.task_number] = i;
    return 0;

}

void execute_encoder_sequential(int argc, char * argv[])
{
    int i, j, items = 0;
    u_int8_t *count_result = malloc(1000000000*sizeof(u_int8_t));
    char *character_result = malloc(1000000000*sizeof(char));
    int a = 0;
    
    for (i = 1; i < argc; i++)
    {
        char *file_in_memory;
        struct stat sb;
        int fd;

        fd = open(argv[i], O_RDONLY);
        if (fd == -1)
        {
            fprintf(stderr, "Failed to open file\n");
            fprintf(stderr, "File %d name:%s\n", i, argv[i]);
        }
        
        if (fstat(fd, &sb) == -1)
        {
            fprintf(stderr, "Failed to get file size\n");
            close(fd);
        }

        file_in_memory = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
 
        if (file_in_memory == MAP_FAILED)
        {
            fprintf(stderr, "Map failed\n");
            close(fd);
        }

        for (j = 0; j < sb.st_size; j++)
        {
            int next = j+1;
            u_int8_t count = 1;
            
        
            while (file_in_memory[j] == file_in_memory[next])
            {
                count ++;
                
                next ++;
                if (!(next < sb.st_size))
                {
                    break;
                }
            }
            
            character_result[a] = file_in_memory[j];
            count_result[a] = count;

            items++;
            a++;
            j = next-1;
        
        }
        
        munmap(file_in_memory, sb.st_size);
        close(fd);

    }
    
    int k, place;
    u_int8_t total_count;
    for (k = 0; k < items; k++)
    {

        total_count = count_result[k];
 
        place = k+1;
        while (character_result[k] == character_result[place])
        {
            total_count = total_count + count_result[place];
          
            place++;
            if (!(place < items))
            {
                break;
            }
        }

        fwrite(&character_result[k], 1, 1, stdout);
        fwrite(&total_count, 1, 1, stdout);

        k = place-1;
        
    }

    free(count_result);
    free(character_result);
}

void submit_task(struct Task task_to_submit) //in submit task we need to signal threads to come and check if there are tasks
{
    pthread_mutex_lock(&queue_mutex);
    task_queue[taskcount] = task_to_submit;
    taskcount++;
    // printf("task submitted! task start = %d, task end = %d\n", task_to_submit.start, task_to_submit.end);
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&cond_queue);
}

void free_arrays(int total_tasks)
{

    int i;
    for (i = 0; i < total_tasks+2; i++)
    {
        free(threading_result[i]);
        
    }

    free(threading_result_length);
    free(threading_result);

}

void *get_task(void *args) //get a task from here (this should be locked as threads should get tasks one at a time)
{
    while (1)
    {
        struct Task current_task;
        pthread_mutex_lock(&queue_mutex);
    
        if (taskcount == 0) //when all tasks are taken
        {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        current_task = task_queue[front];
        front++;
        taskcount--;

        pthread_mutex_unlock(&queue_mutex);
       
        execute_encoder(current_task); //execute task when got a task

    }
    return 0;
    
}

int map_files_tasks(int argc, char * argv[])
{
    int i, total_tasks = 0;
    int task_rank = 0;
    
    
    for (i = 3; i < argc; i++)
    {
        char *file_in_memory;
        struct stat sb;
        int fd;

        fd = open(argv[i], O_RDONLY);
        if (fd == -1)
        {
            fprintf(stderr, "Failed to open file\n");
            fprintf(stderr, "File %d name:%s\n", i, argv[i]);
        }
        
        if (fstat(fd, &sb) == -1)
        {
            fprintf(stderr, "Failed to get file size\n");
            close(fd);
        }
        // printf("File size is: %ld\n", sb.st_size);

        file_in_memory = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

        if (file_in_memory == MAP_FAILED)
        {
            fprintf(stderr, "Map failed\n");
            close(fd);
        }
        close(fd);

        //submit task
        int a = 0, b = 4096;
        size_t total_memory_left = sb.st_size;
        // printf("total memory left = %zu\n", total_memory_left);
        
        while (total_memory_left >= standard)
        {
            struct Task newtask = 
            {
                .start = a,
                .end = b,
                .task_number = task_rank,
                .file_mem = file_in_memory
            };
            submit_task(newtask);
            
            total_tasks++;
            task_rank++;
            total_memory_left -= 4096;
            a = a + 4096;
            b = b + 4096;
        }

        if (total_memory_left > 0)
        {
            
            struct Task newtask = 
            {
                .start = a,
                .end = total_memory_left + a + 1,
                .task_number = task_rank,
                .file_mem = file_in_memory
            };
            submit_task(newtask);

            total_tasks++;
            task_rank++;
        }
    }
    return total_tasks;
}

void stitch_and_print (int total_tasks)
{
    

    // int x, y;
    // printf("total tasks = %d", total_tasks);
    // for (x = 0; x < total_tasks; x++)
    // {
    //     printf("threading_result_length = %d", threading_result_length[x]);
    //     for (y = 0; threading_result[x][y] != NULL; y += 2)
    //     {
            
    //         printf("threading_result[%d][%d] = %c\n", x, y, threading_result[x][y]);
    //         printf("threading_result[%d][%d] = %d\n", x, y+1, threading_result[x][y+1]);

    //     }
    // }


    //拿到character array, traverse through and stitch together. print out the results using char and unsign int both with %c
    int k;

    char temp_char = 0;
    u_int8_t temp_count = 0;

    for (k = 0; k < total_tasks; k++)
    {
        int i = 0;
        
        if (temp_char != 0 && k != 0)
        {
            if (temp_char == threading_result[k][0]) //如果previous end == current start
            {
                temp_count += threading_result[k][1];
                

                fwrite(&temp_char, 1, 1, stdout);
                fwrite(&temp_count, 1, 1, stdout);
                
                // printf("\n0h hey%c%d\n", temp_char, temp_count);
                temp_char = 0;
                temp_count = 0;
                i += 2;

            }
            else //if previous end != current start
            {
            
            
                fwrite(&temp_char, 1, 1, stdout);
                fwrite(&temp_count, 1, 1, stdout);
            

                temp_char = 0; //NULL;
                temp_count = 0;
            }
        }
        int length = threading_result_length[k]; 
        // printf("length = %d", length);
        int j;
        for (j = i; j < length - 2; j += 2) //剩下最后两个
        {
            fwrite(&threading_result[k][j], 1, 1, stdout);
            fwrite(&threading_result[k][j+1], 1, 1, stdout);
            // printf("%c%d\n", threading_result[k][j], threading_result[k][j+1]);
            
    
        }

        
        temp_char = threading_result[k][length-2];
        temp_count = threading_result[k][length-1];
    
    }

    if (temp_char != 0 && temp_count != 0)
    {
        fwrite(&temp_char, 1, 1, stdout);
        fwrite(&temp_count, 1, 1, stdout);
    }
    
}

int main (int argc, char * argv[])
{

    clock_t t;
    t = clock();  
    
    if (strcmp(argv[1], "-j") == 0) // 有optional -j command
    {
        int i, j, total_tasks;
        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_init(&queue_mutex, NULL);
        pthread_cond_init(&cond_queue, NULL);

        int number_of_threads = atoi(argv[2]);
        
        total_tasks = map_files_tasks(argc, argv); //map the files and submit tasks
        // printf("number of tasks %d", total_tasks);

        threading_result = malloc ((total_tasks+2) * sizeof(char*));
        threading_result_length = malloc ((total_tasks+2) * sizeof(int));


        int a;
        for (a = 0; a < total_tasks; a++)
        {
            threading_result[a] = malloc (10000 * sizeof(char)); //malloc 10000 是因为每个4kb最多也就8000多个
        }

        threading_result_length[total_tasks] = 0;//NULL;
        threading_result[total_tasks] = NULL;

        pthread_t threads[number_of_threads];

        for (i = 0; i < number_of_threads; i++)
        {
            if (pthread_create(&threads[i], NULL, &get_task, NULL) != 0) 
            {
                fprintf(stderr, "Error: Cannot create thread # %d\n", i);
            }
        }     

        for (j = 0; j < number_of_threads; j++)
        {
            if (pthread_join(threads[j], NULL) != 0)
            {
                fprintf(stderr, "Error: Cannot join thread # %d\n", j);
            }
        }

        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&queue_mutex);
        pthread_cond_destroy(&cond_queue);
    
        stitch_and_print(total_tasks);

        free_arrays(total_tasks);
    }
    else //没有optional -j command
    {
        execute_encoder_sequential(argc, argv);
    }
    

    // execute_encoder_sequential(argc, argv);


    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    fprintf(stderr, "nyuenc took %f seconds to execute \n", time_taken);
}



