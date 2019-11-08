#include <omp.h>
#include <cstdio>

#include "indices.hpp"


int main(){

    idx_t idx_array[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    Indices x (idx_array, 10);

    printf("%ld\n", x.sum());

    /* Using a Indices type in target */
    #pragma omp target map(tofrom: x)
    {
        printf("%ld\n", x.sum());
    }


    /* Using a ScanIndices type in target */
    ScanIndices si;
    si.start = x;

    printf("%ld\n", si.start.sum());

    #pragma omp target map(tofrom: si)
    {
        printf("%ld\n", si.start.sum());
    }

    return 0;
}
