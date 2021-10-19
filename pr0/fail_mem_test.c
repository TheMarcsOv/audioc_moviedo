/* simple test to experiment with valgrind capacities 
 *
 * Compile as  
 *    gcc -ggdb -o fail_mem_test fail_mem_test.c -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
 * Execute as
 *    ./fail_mem_test
 * Identify which errors are captured and which are not. Correct them. 
 * The key info is the line at which the error occurs. Example
 *      heap-buffer-overflow /media/sf_code/CLASE/SMA/pr0/fail_mem_test.c:47 in main
 * indicates that there is an error in line 47
 * 
 * As you correct one error, another may arise.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

    
int main ()
{

    // PART 1
    int value1 =1;
    int *mem;
    int *mem2;
    int value2[1];

    value2[0] = 2;

    mem=malloc(50*sizeof(int));
    mem2=malloc(50*sizeof(int));
    memset(mem, 0, 50); /* Set memory with zeros */

    printf("value1: %d, value2: %d\n", value1, value2[0]);
    memcpy (mem2, mem, 50*sizeof(int));    
    printf("value1: %d, value2: %d\n", value1, value2[0]);
                
    free(mem);
    free(mem2);

    // PART 2
    int a;
    int array[3]; 
    char *b;


    a = 3;         
    array[2] = 4;   

    b = malloc(2);
    *(b+1)='5';    

    free(b);

    return(0);
}



