

//Development platform: workbench2

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>   /* For timing */
#include <sys/time.h> /* For timing */
#include <sys/resource.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

/****************Global****************************/

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define EPSILON 0.001 /* Termination condition */

char *filename; /* File name of output file */

/* Grid size */
int M = 200;            /* Number of rows */
int N = 200;            /* Number of cols */
long max_its = 1000000; /* Maximum iterations, a safe bound to avoid infinite loop */
double final_diff;      /* Temperature difference between iterations at the end */

/* Thread count */
int thr_count = 2;

/* shared variables between threads */
/*************************************************************/
double **u; /* Previous temperatures */
double **w; /* New temperatures */

// (1) Add your variables here

double max_diff = 0.0; /* Maximum temperature difference */
sem_t start,finish, mutex,sig;   /*transit infomation among threads*/
int StopOrCtn = 0;     /* stop=0, continue=1 */
double **stat;            /* store the worker thread running statistic */

/**************************************************************/

int main(int argc, char *argv[])
{
   int its;                     /* Iterations to converge */
   double elapsed;              /* Execution time */
   struct timeval stime, etime; /* Start and end times */
   struct rusage usage;

   void allocate_2d_array(int, int, double ***);
   void initialize_array(double ***);
   void print_solution(char *, double **);
   int find_steady_state(void);

   /* For convenience of other problem size testing */
   if ((argc == 1) || (argc == 4))
   {
      if (argc == 4)
      {
         M = atoi(argv[1]);
         N = atoi(argv[2]);
         thr_count = atoi(argv[3]);
      } // Otherwise use default grid and thread size
   }
   else
   {
      printf("Usage: %s [ <rows> <cols> <threads ]>\n", argv[0]);
      exit(-1);
   }

   printf("Problem size: M=%d, N=%d\nThread count: T=%d\n", M, N, thr_count);

   /* Create the output file */
   filename = argv[0];
   sprintf(filename, "%s.dat", filename);

   allocate_2d_array(M, N, &u);
   allocate_2d_array(M, N, &w);
   initialize_array(&u);
   initialize_array(&w);

   gettimeofday(&stime, NULL);
   its = find_steady_state();
   gettimeofday(&etime, NULL);

   elapsed = ((etime.tv_sec * 1000000 + etime.tv_usec) - (stime.tv_sec * 1000000 + stime.tv_usec)) / 1000000.0;

   printf("Converged after %d iterations with error: %8.6f.\n", its, final_diff);
   printf("Elapsed time = %8.4f sec.\n", elapsed);

   getrusage(RUSAGE_SELF, &usage);
   printf("Program completed - user: %.4f s, system: %.4f s\n",
          (usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0),
          (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0));
   printf("no. of context switches: vol %ld, invol %ld\n\n",
          usage.ru_nvcsw, usage.ru_nivcsw);

   print_solution(filename, w);
}

/* Allocate two-dimensional array. */
void allocate_2d_array(int r, int c, double ***a)
{
   double *storage;
   int i;
   storage = (double *)malloc(r * c * sizeof(double));
   *a = (double **)malloc(r * sizeof(double *));
   for (i = 0; i < r; i++)
      (*a)[i] = &storage[i * c];
}

/* Set initial and boundary conditions */
void initialize_array(double ***u)
{
   int i, j;

   /* Set initial values and boundary conditions */
   for (i = 0; i < M; i++)
   {
      for (j = 0; j < N; j++)
         (*u)[i][j] = 25.0; /* Room temperature */
      (*u)[i][0] = 0.0;
      (*u)[i][N - 1] = 0.0;
   }

   for (j = 0; j < N; j++)
   {
      (*u)[0][j] = 0.0;
      (*u)[M - 1][j] = 1000.0; /* Heat source */
   }
}

/* Print solution to standard output or a file */
void print_solution(char *filename, double **u)
{
   int i, j;
   char sep;
   FILE *outfile;

   if (!filename)
   {              /* if no filename specified, print on screen */
      sep = '\t'; /* tab added for easier view */
      outfile = stdout;
   }
   else
   {
      sep = '\n'; /* for gnuplot format */
      outfile = fopen(filename, "w");
      if (outfile == NULL)
      {
         printf("Can't open output file.");
         exit(-1);
      }
   }

   /* Print the solution array */
   for (i = 0; i < M; i++)
   {
      for (j = 0; j < N; j++)
         fprintf(outfile, "%6.2f%c", u[i][j], sep);
      fprintf(outfile, "\n"); /* Empty line for gnuplot */
   }
   if (outfile != stdout)
      fclose(outfile);
}

/* Entry function of the worker threads */
void *thr_func(void *arg)
{
   

   int worker_id = *(int *)arg;
   
   int begin_r = (int)(worker_id * M / thr_count);
   int end_r = (int)((worker_id + 1) * M / thr_count - 1);
   double diff; /* Maximum temperature difference within threads */
   struct rusage thr_usage;

   // update begin_r and end_r according to wotker_id
   if (worker_id == 0)
   {
      begin_r++;
   }
   if (worker_id == (thr_count - 1))
   {
      end_r--;
   }

   sem_wait(&start);

   while (StopOrCtn)
   {
      diff = 0.0;
      //update the data of w[][] and diff
      
      for (int r = begin_r; r <= end_r; r++)
      {
         for (int c = 1; c < N - 1; c++)
         {
            w[r][c] = 0.25 * (u[r - 1][c] + u[r + 1][c] + u[r][c - 1] + u[r][c + 1]);
            if (fabs(w[r][c] - u[r][c]) > diff)
               diff = fabs(w[r][c] - u[r][c]);
         }
      }
      
      sem_wait(&mutex);

      if (diff > max_diff)
         max_diff = diff;

      sem_post(&mutex);
          sem_post(&finish);                                                                                                                                  
//tell master thread that this worker is finished
     
      sem_wait(&sig);// waiting for the response from the master thread

   }
   getrusage(RUSAGE_THREAD, &thr_usage);

   stat[worker_id][0]=thr_usage.ru_utime.tv_sec + thr_usage.ru_utime.tv_usec / 1000000.0;
   stat[worker_id][1]=thr_usage.ru_stime.tv_sec + thr_usage.ru_stime.tv_usec / 1000000.0;

   pthread_exit ((void * )&thr_usage);
}

int find_steady_state(void)
{

   int its; /* Iteration count */
   int i, j;
   double **temp;
   struct rusage thr_usage;
   struct rusage func_usage;
   
   allocate_2d_array(thr_count, 2, &stat);

   pthread_t* threads = (pthread_t*)malloc(thr_count * sizeof(pthread_t));
   int* thread_ids = (int*)malloc(thr_count * sizeof(int));

   sem_init(&mutex, 0, 1);

   sem_init(&finish, 0, 0);

   sem_init(&sig, 0, 0);

   sem_init(&start, 0, 0);
  
   //thread creation
   for (i = 0; i < thr_count; i++)
   {
      thread_ids[i] = i;
		pthread_create(&threads[i], NULL, thr_func, (void*)&thread_ids[i]);
   }

   //iteration limit
   for (its = 1; its <= max_its; its++)
   {
      max_diff = 0.0;

      StopOrCtn =1;

      if(its==1){
         for (i = 0; i < thr_count; i++)
         {
            sem_post(&start); 
         }
      } //inform the workers to start for the first time

      for (i = 0; i < thr_count; i++)
      {
         sem_wait(&finish);
      } //wait for all the worker threads to finish

      /* Swap matrix u, w by exchanging the pointers */
      temp = u;
      u = w;
      w = temp;

      /* Terminate if temperatures have converged */
      if (max_diff <= EPSILON)
         break;
      else
      {
         for (i = 0; i < thr_count; i++)
         {
            sem_post(&sig);
         }
      }
   }
   // termination conditions are reached
   StopOrCtn = 0;
     
   

   for (i = 0; i < thr_count; i++)
   {
      
      sem_post(&sig);

   }

   for (i = 0; i < thr_count; i++)
   {
      
      //void * temp= *(struct rusage thr_usage);

      pthread_join(threads[i], (void *)&thr_usage); // retrive info from thread usage

      printf("Thread %d has completed - user: %.4f s, system: %.4f s\n", i,
             stat[i][0],stat[i][1]);
   }

   getrusage(RUSAGE_SELF, &func_usage);
   printf("find_steady_state - user: %.4f s, system: %.4f s\n",
          (func_usage.ru_utime.tv_sec + func_usage.ru_utime.tv_usec / 1000000.0),
          (func_usage.ru_stime.tv_sec + func_usage.ru_stime.tv_usec / 1000000.0));

   final_diff = max_diff; //return the diff value via global variable


   return its;            //return the number of iterations
}
