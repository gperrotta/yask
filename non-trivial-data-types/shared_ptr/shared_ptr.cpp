#include <iostream>
#include <cstdio>
#include <memory>

int main(){

    int *p = new int(10);
    std::shared_ptr<int> x(p);

    // /* Using a pointer as array section */
    // #pragma omp target map(tofrom: p[:1])
    // {
    //     printf("%d\n", *p);
    // }

    /* Using a smart pointer with get */
    int *pp = x.get();
    #pragma omp target map(tofrom: pp[:1])
    {
        *pp = 12;
        printf("%d\n", *pp);
    }
    printf("%d\n", *pp);




    return 0;
}
