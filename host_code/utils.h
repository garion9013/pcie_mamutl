#include <sys/time.h>

void timespec_init(struct timespec *ts);

void timespec_sub(struct timespec *t1, const struct timespec *t2);

void timespec_add(struct timespec *t1, const struct timespec *t2);

void timespec_div(struct timespec *ts, int num);

void mat_transpose_naive(float *in_matrix, float *out_matrix, int num_row, int num_col);

struct timespec cpu_innerproduct(float *in_vector1, float *in_vector2, float *out, int size);

struct timespec cpu_matvec(float *in_matrix, float *in_vector, float *out_vector, int num_row, int num_col);

struct timespec cpu_matmul(float *in_matrix1, float *in_matrix2, float *out_matrix, int num_rowA, int num_colA, int num_colB);

void gettime_overhead();
