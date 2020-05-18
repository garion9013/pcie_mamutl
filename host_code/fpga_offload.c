#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include "device_check.h"
#include "ctrl_register_read.h"
#include "channel_readwrite.h"
#include "utils.h"

#define BRAM_ADDR 0x40000000
#define IP_ADDR 0x43C00000
#define SIZE 64 // if SIZE is changed, HW logic should be changed as well (L_RAM_SIZE, num_operation, etc.)
#define NUM_TRIALS 10000 // number of times trials to measure the average performance in profile_transferSize()
#define NUM_REPEAT 100 // number of times each test will be repeated
#define DIFF_THRESHOLD 0.01 // Threshold of difference between output of FPGA and CPU(ref.)

/* tests the correctness of read and write operation on BRAM
 * "test_size" determines the number of floating-point numbers to be sent back-and-forth
 */
void bram_readwrite_test(uint32_t test_size){

    printf("Performing BRAM read/write test...\n");

    /* memory allocation */
    float *input;
    float *output;
    input = (float *) malloc(sizeof(float) * test_size);
    output = (float *) malloc(sizeof(float) * test_size);

    /* Random Initialization */
    for (int i = 0; i < (int) test_size; i++){
        input[i] = (rand()%10000 + 1) * 0.001f;
    }

    /* Write Data to BRAM */    
    write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR, (0x0004 * test_size), input);
    /* Read Data from BRAM */
    read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR, (0x0004 * test_size), output);

    /* Verify that input and output are identical */
    int test_success = 1;
    for (int j = 0; j < (int) test_size; j++){
        if(input[j] != output[j]){
            printf("%dth number mismatch - input number: %f, output number: %f\n", j, input[j], output[j]);
            test_success = 0;
        }
    }
    if(test_success){
        printf("All %d floating-point numbers are identical! BRAM Read/Write Test Passed!\n", (int) test_size);
    }

    /* cleanup */
    free(input);
    free(output);
}

/* profiles the execution time of data transfer based on single transfer size
 * averages execution time of NUM_TRIALS trials for each transfer size from 1KB to 32KB
 */
void profile_transferSize(){

    printf("Profiling data transfer time...\n");

    /* Memory Allocation */
    float *input_32KB;
    float *output_32KB;
    input_32KB = (float *) malloc((size_t) 0x8000);
    output_32KB = (float *) malloc((size_t) 0x8000);

    struct timespec ts_fpga, ts_fpga_avg;

    /* Random Initialization */
    for (int i = 0; i < 8192; i++){ // 32KB / 4 = 8192
        input_32KB[i] = (rand()%10000 + 1) * 0.001f;
    }

    printf("Number of Trials: %d\n", NUM_TRIALS);

    /* Write Data to BRAM via H2C Channel */
    for (uint32_t j = 0x0400; j < 0x10000; j *=2){ // 1KB to 32KB
        timespec_init(&ts_fpga_avg);
        for (int p=0; p < NUM_TRIALS; p++){
            ts_fpga = write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR, j, input_32KB);
            timespec_add(&ts_fpga_avg, &ts_fpga);
        }
        timespec_div(&ts_fpga_avg, NUM_TRIALS);
        printf("Average WRITE time of %5d bytes: %ld.%09ld seconds\n", j, ts_fpga_avg.tv_sec, ts_fpga_avg.tv_nsec);
    }

    /* Read Data from BRAM via C2H Channel */
    for (uint32_t k = 0x0400; k < 0x10000; k *=2){
        timespec_init(&ts_fpga_avg);
        for (int p=0; p < NUM_TRIALS; p++){
            ts_fpga = read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR, k, output_32KB);
            timespec_add(&ts_fpga_avg, &ts_fpga);
        }
        timespec_div(&ts_fpga_avg, NUM_TRIALS);
        printf("Average READ  time of %5d bytes: %ld.%09ld seconds\n", k, ts_fpga_avg.tv_sec, ts_fpga_avg.tv_nsec); 
    }

    /* cleanup */
    free(input_32KB);
    free(output_32KB);
}

/* profiles the overhead of data transfer of "test_size" (test_size: number of float data)
 * verbose functions are called instead of normal functions
 */
void profile_overhead(uint32_t test_size){

    printf("Profiling data transfer overhead...\n");

    /* Memory Allocation */
    float *input;
    float *output;
    input = (float *) malloc(sizeof(float)*test_size);
    output = (float *) malloc(sizeof(float)*test_size);

    /* Random Initialization */
    for (int i = 0; i < (int) test_size; i++){
        input[i] = (rand()%10000 + 1) * 0.001f;
    }

    // Test NUM_REPEAT times for WRITE
    for (int p = 0; p < NUM_REPEAT; p++){
        write_to_channel_verbose("/dev/xdma0_h2c_0", BRAM_ADDR, test_size*sizeof(float), input);
    }

    // Test NUM_REPEAT times for READ
    for (int p = 0; p < NUM_REPEAT; p++){
        read_from_channel_verbose("/dev/xdma0_c2h_0", BRAM_ADDR, test_size*sizeof(float), output);
    }

    /* cleanup */
    free(input);
    free(output);
}

/* [FPGA should be programmed with vector innerproudct]
 * triggers HW to perform vector innerproduct
 * returns the total execution time
 */ 
struct timespec fpga_innerproduct(float *in_vector1, float *in_vector2, float *out){

    struct timespec ts_start, ts_end;
    uint32_t op_code = 0x5555;

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* Write Data to BRAM */
    write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR, 0x0004*SIZE, in_vector1);
    write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR + 0x0004*SIZE, 0x0004*SIZE, in_vector2);

    /* Send op code to myip */
    write_to_channel("/dev/xdma0_h2c_0", IP_ADDR, 0x0004, &op_code);

    /* Wait until computation is done */
    while(1){
        read_from_channel("/dev/xdma0_c2h_0", IP_ADDR, 0x0004, &op_code);
        if(op_code != 0x5555){
            break;
        }
    }

    /* Read output from BRAM */
    read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR, 0x0004, out);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    timespec_sub(&ts_end, &ts_start);
    return ts_end;
}

/* [FPGA should be programmed with matrix-vector multiplier]
 * triggers HW to perform matrix-vector multiplication (matrix: SIZE*SIZE, vector: SIZE)
 * returns total execution time
 */
struct timespec fpga_matvec(float *in_matrix, float *in_vector, float *out_vector){

    struct timespec ts_start, ts_end;
    uint32_t op_code = 0x5555;

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* Write data to BRAM */
    write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR, 0x0004*SIZE, in_vector);
    write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR + 0x0004*SIZE, 0x0004*SIZE*SIZE, in_matrix); 

    // Send OP Code
    write_to_channel("/dev/xdma0_h2c_0", IP_ADDR, 0x0004, &op_code);

    // Wait until OP is done
    while(1){
        read_from_channel("/dev/xdma0_c2h_0", IP_ADDR, 0x0004, &op_code);
        if(op_code != 0x5555){
            break;
        }
    }
    
    read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR, 0x0004*SIZE, out_vector); // multi PE
//    read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR + 0x0004*(SIZE + SIZE*SIZE), 0x0004*SIZE, out_vector); // single PE

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    return ts_end;
}

/* [FPGA should be programmed with matrix-vector multiplier]
 * profiling version of matvec operation */
void fpga_matvec_verbose(float *in_matrix, float *in_vector, float *out_vector){

    struct timespec ts_global_start, ts_global_end;
    struct timespec ts_write_vector, ts_write_matrix, ts_write_op_code, ts_read_output;
    struct timespec ts_hw_start, ts_hw_end;

    uint32_t op_code = 0x5555;

    clock_gettime(CLOCK_MONOTONIC, &ts_global_start);

    /* Write data to BRAM */
    ts_write_vector = write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR, 0x0004*SIZE, in_vector);
    ts_write_matrix = write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR + 0x0004*SIZE, 0x0004*SIZE*SIZE, in_matrix); 

    // Send OP Code
    ts_write_op_code = write_to_channel("/dev/xdma0_h2c_0", IP_ADDR, 0x0004, &op_code);

    clock_gettime(CLOCK_MONOTONIC, &ts_hw_start);

    // Wait until OP is done
    while(1){
        read_from_channel("/dev/xdma0_c2h_0", IP_ADDR, 0x0004, &op_code);
        if(op_code != 0x5555){
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_hw_end);


    ts_read_output = read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR + 0x0004*(SIZE + SIZE*SIZE), 0x0004*SIZE, out_vector);

    clock_gettime(CLOCK_MONOTONIC, &ts_global_end);
    timespec_sub(&ts_global_end, &ts_global_start);

    timespec_sub(&ts_hw_end, &ts_hw_start);

    printf("Total Runtime : %ld.%09ld seconds\n", ts_global_end.tv_sec, ts_global_end.tv_nsec);
    printf("Vector Write  : %ld.%09ld seconds\n", ts_write_vector.tv_sec, ts_write_vector.tv_nsec);
    printf("Matrix Write  : %ld.%09ld seconds\n", ts_write_matrix.tv_sec, ts_write_matrix.tv_nsec);
    printf("OP code Write : %ld.%09ld seconds\n", ts_write_op_code.tv_sec, ts_write_op_code.tv_nsec);
    printf("HW Runtime    : %ld.%09ld seconds\n", ts_hw_end.tv_sec, ts_hw_end.tv_nsec);
    printf("Output Read   : %ld.%09ld seconds\n", ts_read_output.tv_sec, ts_read_output.tv_nsec);
}

/* [FPGA should be programmed with matrix-vector multiplier]
 * triggers HW(Matrix-Vector) multiple times to perfrom matrix-matrix multiplication (matrix: SIZE*SIZE)
 * returns total execution time
 * NOTE: This function does not call fpga_matvec function, 
 *       because calling fpag_matvec function multiple times will lead to SIZE-1 extra copy of input matrix
 */
struct timespec fpga_matmul(float *in_matrix1, float *in_matrix2, float *out_matrix){

    struct timespec ts_start, ts_end;
    uint32_t op_code = 0x5555;

    float *in_matrix2_t;
    in_matrix2_t = (float *) malloc(sizeof(float)*SIZE*SIZE);

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* Transpose matrix B */
    mat_transpose_naive(in_matrix2, in_matrix2_t, SIZE, SIZE);

    /* for K=0~SIZE-1:  B * A_Row(K) */

    /* Write transposed matrix B to BRAM */
    write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR + 0x0004*SIZE, 0x0004*SIZE*SIZE, in_matrix2_t);

    int k;
    for (k =0; k < SIZE; k++){
        /* Write kth row of matrix A to BRAM */
        write_to_channel("/dev/xdma0_h2c_0", BRAM_ADDR, 0x0004*SIZE, in_matrix1 + SIZE*k);

        op_code = 0x5555;
        write_to_channel("/dev/xdma0_h2c_0", IP_ADDR, 0x0004, &op_code);

        while(1){
            read_from_channel("/dev/xdma0_c2h_0", IP_ADDR, 0x0004, &op_code);
            if(op_code != 0x5555){
                break;
            }
        }
        /* Read kth row of output matrix from BRAM */
//        read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR + 0x0004*(SIZE + SIZE*SIZE), 0x0004*SIZE, out_matrix + SIZE*k); // Single PE
        read_from_channel("/dev/xdma0_c2h_0", BRAM_ADDR, 0x0004*SIZE, out_matrix + SIZE*k); // Multi PE
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    free(in_matrix2_t);

    return ts_end;
}

/* [FPGA should be programmed with matrix-vector multiplier]
 * Naive Version of Large Matrix-Vector multiplication (Tiling)
 * NOTE: This function calls fpga_matvec multiple tiems, without making use of temporal locality
 */
struct timespec fpga_large_matvec_naive(float *in_matrix, float *in_vector, float *out_vector, int num_row, int num_col){

    /* zero initialize out_vector */
    for (int p = 0; p < num_row; p++){
        *(out_vector + p) = 0.0f;
    }

    struct timespec ts_start, ts_end;
    uint32_t op_code = 0x5555;

    /* Create a matrix and vector buffer that will be used for each tile operation */
    float fpga_matrix[SIZE*SIZE];
    float fpga_vector[SIZE];
   
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* loop over tile by row */
    int i;
    for (i = 0; i < num_row; i+=SIZE){
        float out[SIZE] = {0.0f};
        float output_buffer[SIZE] = {0.0f};
        /* for each tile-row, loop over tile by column */
        int j;
        for (j = 0; j < num_col; j+=SIZE){
            /* memcpy for matrix tile */
            int num_row_in_tile = 0;
            // case 1: can fully tile row-wise
            if (num_row - i >= SIZE){
                num_row_in_tile = SIZE;
            }
            // case 2: cannot fully tile row-wise
            else{
                num_row_in_tile = num_row - i;
                // zero-initialze matrix (zero-padding in advance)
                int k;
                for (k = 0; k < SIZE*SIZE; k++){
                    *(fpga_matrix + k) = 0.0f;
                }
            }

            // memcpy the matrix tile based on num_row_in_tile
            int m;
            for (m = 0; m < num_row_in_tile; m++){
                // Case 1: Can fully tile column-wise
                if (num_col - j >= SIZE){
                    memcpy(fpga_matrix + SIZE*m, in_matrix + num_col * i + j + num_col * m, SIZE*sizeof(float));     
                }
                // Case 2: cannot fully tile column-wise
                else{
                    memcpy(fpga_matrix + SIZE*m, in_matrix + num_col * i + j + num_col * m, (num_col - j)*sizeof(float));
                    // zero-pad the rest of column with 0
                    int k;
                    for (k = 0; k < (SIZE - (num_col - j)); k++){
                        *(fpga_matrix + SIZE*m + (num_col - j) + k) = 0.0f;
                    }
                
                }
            }

            /* memcpy for vector tile */
            // case 1: can fully tile column-wise
            if (num_col - j >= SIZE){
                memcpy(fpga_vector, in_vector + j, SIZE*sizeof(float));
            }
            // case 2: cannot fully tile column-wise
            else{
                memcpy(fpga_vector, in_vector + j, (num_col - j)*sizeof(float));
                // zero-pad the rest of input vector with 0
                int k;
                for (k = 0; k < (SIZE - (num_col - j)); ++k){
                    *(fpga_vector + (num_col - j) + k) = 0.0f;
                }
            }

            /* Perform Matrix-Vector Multiplication for given input */
            fpga_matvec(fpga_matrix, fpga_vector, out); 

            int n;
            for (n = 0; n < SIZE; n++){
                output_buffer[n] += out[n];  
            }

        }

        if (num_row - i >= SIZE){
            memcpy(out_vector + i, output_buffer, SIZE*sizeof(float));
        }
        else{
            memcpy(out_vector + i, output_buffer, (num_row - i)*sizeof(float));
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    for(int p =0; p < SIZE*SIZE; p++){
        fpga_matrix[p] = 0.0f;
    }
    for(int p=0; p< SIZE; p++){
        fpga_vector[p] = 0.0f;
    }

    return ts_end;
}

/* [FPGA should be programmed with matrix-vector multiplier]
 * Naive Version of Large Matrix-Matrix Multiplication (Tiling)
 * NOTE: This function calls fpga_large_matvec multiple times, without making use of temporal locality
 */
struct timespec fpga_large_matmul_naive(float *in_matrix1, float *in_matrix2, float *out_matrix, int num_rowA, int num_colA, int num_colB){

    float *in_matrix2_t;
    in_matrix2_t = (float *)malloc(sizeof(float)*num_colA*num_colB);
    float *out_matrix_t;
    out_matrix_t = (float *)malloc(sizeof(float)*num_rowA*num_colB);

    for (int n = 0; n < num_rowA*num_colB; n++){
        *(out_matrix_t + n) = 0.0f;
    }


    struct timespec ts_start, ts_end;


    clock_gettime(CLOCK_MONOTONIC, &ts_start);
 
    mat_transpose_naive(in_matrix2, in_matrix2_t, num_colA, num_colB);

    int i;
    for(i = 0; i < num_colB; i++){
        fpga_large_matvec_naive(in_matrix1, in_matrix2_t + num_colA * i, out_matrix_t + num_rowA * i, num_rowA, num_colA);
    }

    mat_transpose_naive(out_matrix_t, out_matrix, num_colB, num_rowA);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    free(in_matrix2_t);
    free(out_matrix_t);

    return ts_end;
}

/* [FPGA should be programmed with matrix-vector multiplier]
 * Naive Version of Large Matrix-Matrix Multiplication (Tiling)
 * NOTE: This function calls fpga_matmul multiple times, without making use of temporal locality
 */
struct timespec fpga_large_matmul_naive2(float *in_matrix1, float *in_matrix2, float *out_matrix, int num_rowA, int num_colA, int num_colB){
 
    struct timespec ts_start, ts_end;
    uint32_t op_code = 0x5555;

    /* Create a matrix buffer that will be used for each tile operation */
    float fpga_matrix1[SIZE*SIZE];
    float fpga_matrix2[SIZE*SIZE];
   
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* ith tile row of matrix1 */
    for (int i = 0; i < num_rowA; i+=SIZE){


        // first check whether we can fully tile in terms of i
        int tilesize_i = 0;
        if (num_rowA - i >= SIZE){
            tilesize_i = SIZE;
        }
        else{
            tilesize_i = num_rowA - i;
        }

        /* jth tile column of matrix2 */
        for (int j = 0; j < num_colB; j+=SIZE){

            // then check whether we can fully tile in terms of j
            int tilesize_j = 0;
            if (num_colB - j >= SIZE){
                tilesize_j = SIZE;
            }
            else{
                tilesize_j = num_colB - j;
            }

            float out[SIZE*SIZE] = {0.0f}; // save output of each FPGA computation
            float output_buffer[SIZE*SIZE] = {0.0f}; // buffer to sum k "out" for each (i, j) pair

            /* kth tile of multiplying ith tile row and jth tile column */
            for (int k =0; k< num_colA; k+=SIZE){

                // zero initialize matrix buffer
                for (int p =0; p < SIZE*SIZE; p++){
                    fpga_matrix1[p] = 0.0f;
                    fpga_matrix2[p] = 0.0f;
                }

                // finally check whether we can fully tile in terms of k
                int tilesize_k = 0;
                if (num_colA - k >= SIZE){
                    tilesize_k = SIZE;
                }
                else{
                    tilesize_k = num_colA - k;
                }

                /* memcpy the according tile to matrix buffer */

                // memcpy tile from matrix1
                for (int p = 0; p < tilesize_i; p++){
                    memcpy(fpga_matrix1 + SIZE*p, in_matrix1 + num_colA * i + num_colA * p + k, sizeof(float)*tilesize_k);
                }
                // memcpy tile from matrix2
                for (int p = 0; p < tilesize_k; p++){
                    memcpy(fpga_matrix2 + SIZE*p, in_matrix2 + num_colB * k + num_colB * p + j, sizeof(float)*tilesize_j);
                }

                /* invoke matrix-matrix multiplication */
                fpga_matmul(fpga_matrix1, fpga_matrix2, out);

                for (int p = 0; p < SIZE*SIZE; p++){
                    output_buffer[p] += out[p];
                }

            }

            /* save result to output */
            for (int p = 0; p < tilesize_i; p++){
                memcpy(out_matrix + num_colB * i + num_colB * p + j, output_buffer + SIZE*p, sizeof(float)*tilesize_j);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    return ts_end;
}

int main(void){

    /* Making sure that the device is recognized */
    device_check();

    /* Check the number of enabled channels by reading xmda control register values */
    int num_en_h2c = check_h2c_channels();
    printf("Number of Enabled H2C channels: %d\n", num_en_h2c);
    if (num_en_h2c == 0){
        printf("ERROR: No PCIe DMA H2C channels were identified\n");
        exit(1);
    }

    int num_en_c2h = check_c2h_channels();
    printf("Number of Enabled C2H channels: %d\n", num_en_c2h);
    if (num_en_c2h == 0){
        printf("ERROR: No PCIe DMA C2H channels were identified\n");
        exit(1);
    }

    /* Functionality Tests */
    srand(time(NULL)); // random seed

    /* 1. Perform BRAM read/write test */
    bram_readwrite_test(8192);

//    gettime_overhead();

    /* 2. Performance Profiling */
    profile_transferSize();

    /* 3. Overhead Profiling */
//    profile_overhead(SIZE*SIZE); //


    /* 4. Vector Innerproduct Test */
//    fpga_innerproduct();

    /* variable setup and initialization for following tests*/    
    float in_vector[SIZE];
    float in_matrix1[SIZE*SIZE];
    float in_matrix2[SIZE*SIZE];
    float cpu_out_vector[SIZE];
    float fpga_out_vector[SIZE];
    float cpu_out_matrix[SIZE*SIZE];
    float fpga_out_matrix[SIZE*SIZE];
    
    struct timespec ts_fpga, ts_cpu;
    struct timespec ts_fpga_avg, ts_cpu_avg;
    int success_flag = 1;

    /* 5. Matrix-Vector Multiplication Test */
    printf("Performing Matrix-Vector Multiplication Test...\n");
    timespec_init(&ts_fpga_avg);
    timespec_init(&ts_cpu_avg);

    for (int p =0; p < NUM_REPEAT; p++){

        /* random initialization for input, zero initialization for output */        
        for (int i=0; i < SIZE; i++){
            in_vector[i] = (rand()%10000 + 1) * 0.001f;
            cpu_out_vector[i] = 0.0f;
            fpga_out_vector[i] = 0.0f;
        }

        for (int i=0; i < SIZE*SIZE; i++){
            in_matrix1[i] = (rand()%100000 + 1) * 0.001f;
        }

        success_flag = 1;
        ts_fpga = fpga_matvec(in_matrix1, in_vector, fpga_out_vector);
        ts_cpu = cpu_matvec(in_matrix1, in_vector, cpu_out_vector, SIZE, SIZE);

        timespec_add(&ts_fpga_avg, &ts_fpga);
        timespec_add(&ts_cpu_avg, &ts_cpu);

        for (int j=0; j < SIZE; j++){
            if(abs((fpga_out_vector[j] - cpu_out_vector[j]))/cpu_out_vector[j] > DIFF_THRESHOLD){
                printf("%2dth element Differ - FPGA: %f CPU: %f Diff: %f\n", j, fpga_out_vector[j], cpu_out_vector[j], (fpga_out_vector[j] - cpu_out_vector[j])/cpu_out_vector[j]);
                success_flag = 0;
            }
        }

        printf("Matrix-Vector Multiplication(FPGA): %ld.%09ld seconds\n", ts_fpga.tv_sec, ts_fpga.tv_nsec);
        printf("Matrix-Vector Multiplication(CPU) : %ld.%09ld seconds\n", ts_cpu.tv_sec, ts_cpu.tv_nsec);
        if (success_flag){
            printf("Matrix-Vector Multiplication Test PASSED!\n");
        }
        else{
            printf("Matrix-Vector Multiplication Test FAILED!\n");
            exit(1);
        }

    }

    timespec_div(&ts_fpga_avg, NUM_REPEAT);
    timespec_div(&ts_cpu_avg, NUM_REPEAT);

    printf("Average time (FPGA): %ld.%09ld seconds\n", ts_fpga_avg.tv_sec, ts_fpga_avg.tv_nsec);
    printf("Average time (CPU) : %ld.%09ld seconds\n", ts_cpu_avg.tv_sec, ts_cpu_avg.tv_nsec);

    /* 5-2. Matrix-Vector Multiplication in Verbose Mode */
    for (int p=0; p <NUM_REPEAT; p++){
        printf("%dth iteration...\n", p);
        fpga_matvec_verbose(in_matrix1, in_vector, fpga_out_vector);
    }

    /* 6. Matirx-Matrix Multiplication Test */
    printf("Performing Matrix-Matrix Multiplication Test...\n");
    timespec_init(&ts_fpga_avg);
    timespec_init(&ts_cpu_avg);

    for (int p =0; p < NUM_REPEAT; p++){
        /* random initialization for input, zero initialization for output */        
        for (int i=0; i < SIZE*SIZE; i++){
            in_matrix1[i] = (rand()%10000 + 1) * 0.001f;
            in_matrix2[i] = (rand()%10000 + 1) * 0.001f;
            cpu_out_matrix[i] = 0.0f;
            fpga_out_matrix[i] = 0.0f;
        }

        success_flag = 1;
        ts_fpga = fpga_matmul(in_matrix1, in_matrix2, fpga_out_matrix);
        ts_cpu = cpu_matmul(in_matrix1, in_matrix2, cpu_out_matrix, SIZE, SIZE, SIZE);
    
        timespec_add(&ts_fpga_avg, &ts_fpga);
        timespec_add(&ts_cpu_avg, &ts_cpu);

        for (int k=0; k < SIZE*SIZE; k++){
            if(abs((fpga_out_matrix[k] - cpu_out_matrix[k]))/cpu_out_matrix[k] > DIFF_THRESHOLD){
                printf("%4dth element Differ - FPGA: %f CPU: %f Diff: %f\n", k, fpga_out_matrix[k], cpu_out_matrix[k], (fpga_out_matrix[k] - cpu_out_matrix[k])/cpu_out_matrix[k]);
                success_flag = 0;
            }
         }

         printf("Matrix-Matrix Multiplication(FPGA): %ld.%09ld seconds\n", ts_fpga.tv_sec, ts_fpga.tv_nsec);
         printf("Matrix-Matrix Multiplication(CPU) : %ld.%09ld seconds\n", ts_cpu.tv_sec, ts_cpu.tv_nsec);
        if (success_flag){
            printf("Matrix-Matrix Multiplication Test PASSED!\n");
        }
        else{
            printf("Matrix-Matrix Multiplication Test FAILED!\n");
            exit(1);
        }

    }

    timespec_div(&ts_fpga_avg, NUM_REPEAT);
    timespec_div(&ts_cpu_avg, NUM_REPEAT);

    printf("Average time (FPGA): %ld.%09ld seconds\n", ts_fpga_avg.tv_sec, ts_fpga_avg.tv_nsec);
    printf("Average time (CPU) : %ld.%09ld seconds\n", ts_cpu_avg.tv_sec, ts_cpu_avg.tv_nsec);

    /* variable setup and initialization for following tests */
    float in_large_vector[512];
    float in_large_matrix1[784*1024];
    float in_large_matrix2[75*1024];
    float cpu_out_large_vector[784];
    float fpga_out_large_vector[784];
    float cpu_out_large_matrix[32*1024];
    float fpga_out_large_matrix[32*1024];

    /* 7. Large Matrix-Vector Multiplication Test */
    printf("Performing Large Matrix-Vector Multiplication Test...\n");
    timespec_init(&ts_fpga_avg);
    timespec_init(&ts_cpu_avg);

    for (int p =0; p < NUM_REPEAT; p++){

        /* random initialization for input, zero initialization for output */
        for (int m=0; m < 512; m++){
            in_large_vector[m] = (rand()%10000 + 1) * 0.001f;
        }
        for (int m=0; m < 784; m++){
            cpu_out_large_vector[m] = 0.0f;
            fpga_out_large_vector[m] = 0.0f;
        }
        for (int m=0; m < 784*1024; m++){
            in_large_matrix1[m] = (rand()%10000 + 1) * 0.001f;
        }

        success_flag = 1;

        ts_fpga = fpga_large_matvec_naive(in_large_matrix1, in_large_vector, fpga_out_large_vector, 784, 512);
        ts_cpu = cpu_matvec(in_large_matrix1, in_large_vector, cpu_out_large_vector, 784, 512);

        timespec_add(&ts_fpga_avg, &ts_fpga);
        timespec_add(&ts_cpu_avg, &ts_cpu);
 
        for (int n = 0; n < 784; n++){
            if(abs((fpga_out_large_vector[n] - cpu_out_large_vector[n]))/cpu_out_large_vector[n] > DIFF_THRESHOLD){
                printf("%4dth element Differ - FPGA: %f CPU: %f Diff: %f\n", n, fpga_out_large_vector[n], cpu_out_large_vector[n], (fpga_out_large_vector[n] - cpu_out_large_vector[n])/cpu_out_large_vector[n]);
               success_flag = 0;
            }
        }
    
        printf("Large Matrix-Vector Multiplication(FPGA): %ld.%09ld seconds\n", ts_fpga.tv_sec, ts_fpga.tv_nsec);
        printf("Large Matrix-Vector Multiplication(CPU) : %ld.%09ld seconds\n", ts_cpu.tv_sec, ts_cpu.tv_nsec);
        if (success_flag){
            printf("Large Matrix-Vector Multiplication Test PASSED!\n");
        }
        else{
            printf("Large Matrix-Vector Multiplication Test FAILED!\n");
            exit(1);
        }
    }

    timespec_div(&ts_fpga_avg, NUM_REPEAT);
    timespec_div(&ts_cpu_avg, NUM_REPEAT);

    printf("Average time (FPGA): %ld.%09ld seconds\n", ts_fpga_avg.tv_sec, ts_fpga_avg.tv_nsec);
    printf("Average time (CPU) : %ld.%09ld seconds\n", ts_cpu_avg.tv_sec, ts_cpu_avg.tv_nsec);


    /* 8. Large Matrix-Matrix Multiplication Test */ 
    printf("Performing Large Matrix-Matrix Multiplication Test...\n");
    timespec_init(&ts_fpga_avg);
    timespec_init(&ts_cpu_avg);

    for (int p =0; p < NUM_REPEAT; p++){

        /* random initialization for input, zero initialization for output */
        for (int m=0; m < 32*75; m++){
            in_large_matrix1[m] = (rand()%10000 + 1) * 0.001f;
        }
        for (int m=0; m < 75*1024; m++){
            in_large_matrix2[m] = (rand()%10000 + 1) * 0.001f;
        }
        for (int m=0; m < 32*1024; m++){
            cpu_out_large_matrix[m] = 0.0f;
            fpga_out_large_matrix[m] = 0.0f;
        }
 
        success_flag = 1;

        ts_fpga = fpga_large_matmul_naive2(in_large_matrix1, in_large_matrix2, fpga_out_large_matrix, 32, 75, 1024);
        ts_cpu = cpu_matmul(in_large_matrix1, in_large_matrix2, cpu_out_large_matrix, 32, 75, 1024);

        timespec_add(&ts_fpga_avg, &ts_fpga);
        timespec_add(&ts_cpu_avg, &ts_cpu);
 
        for (int q = 0; q < 32*1024; q++){
           if(abs((fpga_out_large_matrix[q] - cpu_out_large_matrix[q]))/cpu_out_large_matrix[q] > DIFF_THRESHOLD){
               printf("%5dth element Differ - FPGA: %f CPU: %f Diff: %f\n", q, fpga_out_large_matrix[q], cpu_out_large_matrix[q], (fpga_out_large_matrix[q] - cpu_out_large_matrix[q])/cpu_out_large_matrix[q]);
              success_flag = 0; 
            }
        }

        printf("Large Matrix-Matrix Multiplication(FPGA): %ld.%09ld seconds\n", ts_fpga.tv_sec, ts_fpga.tv_nsec);
        printf("Large Matrix-Matrix Multiplication(CPU) : %ld.%09ld seconds\n", ts_cpu.tv_sec, ts_cpu.tv_nsec);    
        if (success_flag){
            printf("Large Matrix-Matrix Multiplication Test PASSED!\n");
        }
        else{
            printf("Large Matrix-Matrix Multiplication Test FAILED!\n");
            exit(1);
        }
    }

    timespec_div(&ts_fpga_avg, NUM_REPEAT);
    timespec_div(&ts_cpu_avg, NUM_REPEAT);

    printf("Average time (FPGA): %ld.%09ld seconds\n", ts_fpga_avg.tv_sec, ts_fpga_avg.tv_nsec);
    printf("Average time (CPU) : %ld.%09ld seconds\n", ts_cpu_avg.tv_sec, ts_cpu_avg.tv_nsec);

    printf("Passed all functionality test!\n");

    return 0;
}
