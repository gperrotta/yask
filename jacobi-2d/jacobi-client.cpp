#include <cstdio>
#include <iostream>
#include <cmath>
#include <cassert>
#include <set>
#include <vector>

#include "yask_kernel_api.hpp"


/* Stencil compile time definition

u( {t, x, y} )

*/
using namespace std;
using namespace yask;

#define SIZE 10
#define STEPS 100

// Pretty prints as 2D Matrix
void print_array(double *a, long int row, long int col){
  for (long i = 0; i < row; i++){
    for (long j = 0; j < col; j++)
      printf("%.2lf ", a[i * col + j]);
    printf("\n");
  }
  printf("\n");
}

// Pretty prints var as 2D Matrix
void print_array(yk_var_ptr &var, long int t, long int row, long int col){
  for (long i = 0; i < row; i++){
    for (long j = 0; j < col; j++)
      printf("%.2lf ", var->get_element({t, i, j}));
    printf("\n");
  }
  printf("\n");
}

void init_var(std::shared_ptr<yask::yk_var> &var, long int row, long int col){
  for (long i = 0; i < row; i++){
    for (long j = 0; j < col; j++)
      var->set_element(sqrt(i * SIZE + j), {0, i, j}, true);
  }
}

int main(int argc, char *argv[]){

  //// Test

  double *in = (double*) calloc((SIZE+2) * (SIZE+2), sizeof(double));
  double *scratch = (double*) calloc((SIZE+2) * (SIZE+2), sizeof(double));
  for (int i = 1; i <= SIZE; i++){
    for (int j = 1; j <= SIZE; j++){
      in[i * (SIZE+2) + j] =  sqrt((i-1) * (SIZE) + (j-1));
    }
  }

  for (long int s = 0; s < STEPS; s++){
    for (long int i = 1; i <= SIZE; i++){
      for (long int j = 1; j <= SIZE; j++){
        scratch[i * (SIZE+2) + j] = 0.2 * (in[i * (SIZE+2) + j] +
                                            in[(i+1) * (SIZE+2) + j] +
                                            in[(i-1) * (SIZE+2) + j] +
                                            in[i * (SIZE+2) + j - 1] +
                                            in[i * (SIZE+2) + j + 1]);
      }
    }
    // copy-back
    for (long int i = 1; i <= SIZE; i++)
      for (long int j = 1; j <= SIZE; j++)
        in[i * (SIZE+2) + j] = scratch[i * (SIZE+2) + j];
  }

  //// YASK

  // solution setup
  yk_factory kfac;
  auto env = kfac.new_env();
  auto soln = kfac.new_solution(env);

  // make a set of dimension names
  auto dim_names = soln->get_domain_dim_names();

  set<string> domain_dim_set;
  for (auto dname : dim_names) {
      domain_dim_set.insert(dname);
  }



  // setting pad and domain sizes
  for (auto dim_name: dim_names) {
    // cout << dim_name << " ";
    soln->set_overall_domain_size(dim_name, SIZE);
    soln->set_min_pad_size(dim_name, 1);
  }

  // Allocate memory for any vars that do not have storage set.
  // Set other data structures needed for stencil application.
  soln->prepare_solution();

  // init the vars


  for (auto var : soln->get_vars()) {

    // // cout << "Init var '" << var->get_name() << "':\n";
    //
    // // Done with fixed-size vars.
    // if (var->is_fixed_size())
    //     continue;
    //
    // // processing each dimension of var
    // vector<idx_t> first_indices, last_indices;
    //
    // for (auto dname : var->get_dim_names()){
    //   idx_t first_idx = 0, last_idx = 0;
    //
    //   // domain dim
    //   if (domain_dim_set.count(dname)) {
    //     first_idx = 0;
    //     last_idx = SIZE;
    //   }
    //   // Set indices for valid time-steps.
    //   else if (dname == soln->get_step_dim_name()){
    //     first_idx = var->get_first_valid_step_index();
    //     last_idx = var->get_last_valid_step_index();
    //     assert(last_idx - first_idx + 1 == var->get_alloc_size(dname));
    //   }
    //   // Misc dimension
    //   else {
    //     first_idx = var->get_first_misc_index(dname);     // Set indices to set all allowed values.
    //     last_idx = var->get_last_misc_index(dname);
    //     assert(last_idx - first_idx + 1 == var->get_alloc_size(dname));
    //   }
    //
    //   // TODO: test if this is the same as checking if var == "C"
    //
    //   // Add indices to index vectors.
    //   first_indices.push_back(first_idx);
    //   last_indices.push_back(last_idx);
    // }

    if (var->get_name() == "C")
      continue;

    init_var(var, SIZE, SIZE);

    // cout << "var '" << var->get_name() << "':\n";
    // print_array(var, 0, SIZE, SIZE);

  } // for var : get_vars()

  // print_array(buffer, SIZE, SIZE);

  soln->run_solution(0, STEPS);

  yk_var_ptr var = soln->get_var("U");

  // PRINT OUTPUT
  cout << "YASK:\n";
  print_array(var, STEPS, SIZE, SIZE);
  cout << "Handmade:\n";
  print_array(in, SIZE+2, SIZE+2);

  soln->end_solution();

  return 0;
}
