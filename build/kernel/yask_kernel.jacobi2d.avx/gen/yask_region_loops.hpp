/*
 * 3-D var-scanning code.
 * Generated automatically from the following pseudo-code:
 *
 * N = 2;
 * omp
 * loop (1..N - 1)
 * {
 *   call (calc_block (bp, region_shift_num, nphases, phase, rank_idxs));
 * }
 *
 */
#ifndef OMP_PRAGMA
#define OMP_PRAGMA _Pragma("omp parallel for schedule(dynamic,1) proc_bind(spread)")
#endif

 // 'ScanIndices region_idxs' must be set before the following code.
{

  // ** Begin scan over dimensions 1, 2. **

  // Alignment must be less than or equal to stride size.
  const idx_t adj_align_1 =
    std::min (region_idxs.align[1], region_idxs.stride[1]);

  // Aligned beginning point such that (region_idxs.begin[1] -
  // region_idxs.stride[1]) < aligned_begin_1 <= region_idxs.begin[1].
  const idx_t aligned_begin_1 =
    yask::round_down_flr (region_idxs.begin[1] - region_idxs.align_ofs[1],
			  adj_align_1) + region_idxs.align_ofs[1];

  // Number of iterations to get from aligned_begin_1 to (but not including)
  // region_idxs.end[1], striding by region_idxs.stride[1]. This value is
  // rounded up because the last iteration may cover fewer than
  // region_idxs.stride[1] strides.
  const idx_t num_iters_1 =
    yask::ceil_idiv_flr (region_idxs.end[1] - aligned_begin_1,
			 region_idxs.stride[1]);

  // Alignment must be less than or equal to stride size.
  const idx_t adj_align_2 =
    std::min (region_idxs.align[2], region_idxs.stride[2]);

  // Aligned beginning point such that (region_idxs.begin[2] -
  // region_idxs.stride[2]) < aligned_begin_2 <= region_idxs.begin[2].
  const idx_t aligned_begin_2 =
    yask::round_down_flr (region_idxs.begin[2] - region_idxs.align_ofs[2],
			  adj_align_2) + region_idxs.align_ofs[2];

  // Number of iterations to get from aligned_begin_2 to (but not including)
  // region_idxs.end[2], striding by region_idxs.stride[2]. This value is
  // rounded up because the last iteration may cover fewer than
  // region_idxs.stride[2] strides.
  const idx_t num_iters_2 =
    yask::ceil_idiv_flr (region_idxs.end[2] - aligned_begin_2,
			 region_idxs.stride[2]);

  // Number of iterations in dimensions 1, 2
  const idx_t num_iters_1_2 = num_iters_1 * num_iters_2;

  // Inner loop.


  // Distribute iterations among OpenMP threads.
    #pragma omp parallel for schedule(dynamic,1) proc_bind(spread)
    for (idx_t loop_index_1_2 = 0; loop_index_1_2 < num_iters_1_2; loop_index_1_2++){

      // Zero-based, unit-stride index for dimension 1.
      idx_t index_1 = loop_index_1_2 / num_iters_2;

      // Zero-based, unit-stride index for dimension 2.
      idx_t index_2 = (loop_index_1_2) % num_iters_2;

      // This value of index_1 covers dimension 1 from start_1 to (but not
      // including) stop_1.
      idx_t start_1 = std::max(aligned_begin_1 + (index_1 * region_idxs.stride[1]), region_idxs.begin[1]);
      idx_t stop_1 = std::min(aligned_begin_1 + ((index_1 + 1) * region_idxs.stride[1]), region_idxs.end[1]);

      // This value of index_2 covers dimension 2 from start_2 to (but not
      // including) stop_2.
      idx_t start_2 = std::max(aligned_begin_2 + (index_2 * region_idxs.stride[2]), region_idxs.begin[2]);
      idx_t stop_2 = std::min(aligned_begin_2 + ((index_2 + 1) * region_idxs.stride[2]), region_idxs.end[2]);

      // Local copy of indices for function calls.
      ScanIndices local_indices(region_idxs);
      local_indices.start[1] = start_1;
      local_indices.stop[1] = stop_1;
      local_indices.index[1] = index_1;
      local_indices.num_indices[1] = num_iters_1;
      local_indices.start[2] = start_2;
      local_indices.stop[2] = stop_2;
      local_indices.index[2] = index_2;
      local_indices.num_indices[2] = num_iters_2;
      local_indices.linear_indices = num_iters_1_2;
      local_indices.linear_index = loop_index_1_2;

      #pragma omp target map(tofrom: bp, region_shift_num, nphases, phase, rank_idxs, local_indices)
      calc_block(bp, region_shift_num, nphases, phase, rank_idxs, local_indices);
    }
}

#undef OMP_PRAGMA

 // End of generated code.
