#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#define BILLION 1000000000
#define MILLION 1000000

/* explicitly initialize timespec to 0 */

void timespec_init(struct timespec *ts){
    ts->tv_sec = 0;
    ts->tv_nsec = 0;
}

/* Subtract timespec t2 from t1
 *
 * Both t1 and t2 must already be normalized
 * i.e. 0 <= nsec < 1000000000 
 */
void timespec_sub(struct timespec *t1, const struct timespec *t2)
{
    assert(t1->tv_nsec >= 0);
    assert(t1->tv_nsec < BILLION);
    assert(t2->tv_nsec >= 0);
    assert(t2->tv_nsec < BILLION);

    t1->tv_sec -= t2->tv_sec;
    t1->tv_nsec -= t2->tv_nsec;

    if(t1->tv_nsec < 0){
        t1->tv_sec--;
        t1->tv_nsec += BILLION;
    }
}


/* Add timespec t2 to t1
 *
 * Both t1 and t2 must already be normalized
 * i.e. 0 <= nsec < 1000000000
 */
void timespec_add(struct timespec *t1, const struct timespec *t2){

    assert(t1->tv_nsec >= 0);
    assert(t1->tv_nsec < BILLION);
    assert(t2->tv_nsec >= 0);
    assert(t2->tv_nsec < BILLION);

    t1->tv_sec += t2->tv_sec;
    t1->tv_nsec += t2->tv_nsec;

    if(t1->tv_nsec >= BILLION){
        t1->tv_sec++;
        t1->tv_nsec -= BILLION;
    }
}

/* Divide timespec ts by num
 *
 * ts must already be normalized
 * i.e. 0 <= nsec < 1000000000
 */

void timespec_div(struct timespec *ts, int num){

    assert(ts->tv_nsec >= 0);
    assert(ts->tv_nsec < BILLION);
    assert(num > 0);

    double below_zero = (ts->tv_sec / (double) num) - (ts->tv_sec / num);
    ts->tv_sec /= num;
    ts->tv_nsec /= num;
    ts->tv_nsec += below_zero * BILLION; // this summation cannot exceed BILLION
}


/* Naive matrix transpose */
void mat_transpose_naive(float *in_matrix, float *out_matrix, int num_row, int num_col){

    int i;
    for (i = 0; i < num_row; i++){
        int j;
        for (j = 0; j < num_col; j++){
            out_matrix[j*num_row + i]  = in_matrix[i*num_col + j];
        }
    }
}


/* Matrix transpose with tiling */
//void mat_transpose_tiling(float *in_matrix, float *out_matrix, int in_row, int in_col){
//
//
//}

/* Reference CPU code for vector innerproduct */
struct timespec cpu_innerproduct(float *in_vector1, float *in_vector2, float *out, int size){

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    int i;
    for (i = 0; i < size; i++){
        *out += in_vector1[i] * in_vector2[i];
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    return ts_end;
}

/* Reference CPU code for matrix-vector multiplication */
struct timespec cpu_matvec(float *in_matrix, float *in_vector, float *out_vector, int num_row, int num_col){

    struct timespec ts_start, ts_end;

    for (int p =0; p < num_row; p++){
        out_vector[p] = 0.0f;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    int i;
    for (i=0; i < num_row; i++){
        int j;
        for(j=0; j < num_col; j++){
            out_vector[i] += in_matrix[num_col*i + j] * in_vector[j];
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    return ts_end;
}

/* Reference CPU code for matrix-matrix multiplication */
struct timespec cpu_matmul(float *in_matrix1, float *in_matrix2, float *out_matrix, int num_rowA, int num_colA, int num_colB){

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    for(int p=0; p < num_rowA*num_colB; p++){
        *(out_matrix + p) = 0.0f;
    }

    int i;
    for (i = 0; i < num_rowA; i++){
        int j;
        for (j = 0; j < num_colB; j++){
             int k;
             for (k = 0; k < num_colA; k++){
                 out_matrix[i*num_colB + j] += in_matrix1[i*num_colA + k] * in_matrix2[k*num_colB + j];
             }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);
    return ts_end;
}

/* Computes average overhead of clock_gettime function */
void gettime_overhead(){
    struct timespec ts_avg;
    timespec_init(&ts_avg);
    for (int p =0; p < MILLION*10; p++){
        struct timespec ts_start, ts_mid, ts_end;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        clock_gettime(CLOCK_MONOTONIC, &ts_mid);
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        
        timespec_sub(&ts_end, &ts_start);
        //printf("clock_gettime overhead: %ld.%09ld seconds\n", ts_end.tv_sec, ts_end.tv_nsec);
        timespec_add(&ts_avg, &ts_end);
    }
    timespec_div(&ts_avg, MILLION*10);
    printf("Average clock_gettime overhead: %ld.%09ld seconds\n", ts_avg.tv_sec, ts_avg.tv_nsec);
}
