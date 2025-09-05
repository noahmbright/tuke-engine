#include "statistics.h"
#include "tuke_engine.h"

#include <assert.h>

#define SCRATCH_THRESHOLD (1024)

void init_alias_method(AliasMethod *alias_method, u32 n, const f32 *weights,
                       u64 seed) {
  alias_method->n = n;
  alias_method->rng = create_rng(seed);

  // set internal pointers correctly
  if (n > STATIC_ALIAS_THRESHOLD) {
    alias_method->dynamic_buffer =
        (u8 *)malloc(n * (sizeof(f32) + sizeof(u32)));
    assert(alias_method->dynamic_buffer);
    alias_method->probability_table = (f32 *)alias_method->dynamic_buffer;
    alias_method->alias_table =
        (u32 *)(alias_method->dynamic_buffer + n * sizeof(f32));
  } else {
    alias_method->dynamic_buffer = NULL;
    alias_method->probability_table = alias_method->static_probability_table;
    alias_method->alias_table = alias_method->static_alias_table;
  }

  // initialize alias table
  // initialize scratch buffers
  // small and large are stacks. small gets the indices of probabilities less
  // than 1 "probabilities" means np_i, with p_i = w_i/ sum_j w_j
  // these can exceed 1, these fall into [0, n]
  i32 static_scratch_buffer[2 * SCRATCH_THRESHOLD];
  i32 *small, *large;
  i32 *scratch_buffer = NULL;
  u32 small_top = 0, large_top = 0;

  if (n > SCRATCH_THRESHOLD) {
    scratch_buffer = (i32 *)malloc(2 * n * sizeof(i32));
    small = scratch_buffer;
    large = scratch_buffer + n;
  } else {
    small = static_scratch_buffer;
    large = static_scratch_buffer + n;
  }

  // normalize weights and default aliases
  f32 accumulated_weight = 0.0f;
  for (u32 i = 0; i < n; i++) {
    alias_method->alias_table[i] = i;
    accumulated_weight += weights[i];
  }

  f32 scale = (f32)n / accumulated_weight;
  // put indices of over/under full weights onto the corresponding stack
  for (u32 i = 0; i < n; i++) {
    alias_method->probability_table[i] = scale * weights[i];
    if (alias_method->probability_table[i] > 1.0f) {
      large[large_top++] = i;
    } else {
      small[small_top++] = i;
    }
  }

  // balance stacks by moving excess probability in l to s and adding aliases
  // into the small indices
  while (small_top > 0 && large_top > 0) {
    i32 i_small = small[--small_top];
    i32 i_large = large[--large_top];

    // set alias, let excess in the large bucket spill here
    alias_method->alias_table[i_small] = i_large;

    // the excess probability of getting l moved into s
    f32 delta_p = 1.0f - alias_method->probability_table[i_small];
    alias_method->probability_table[i_large] =
        alias_method->probability_table[i_large] - delta_p;

    if (alias_method->probability_table[i_large] < 1.0f) {
      small[small_top++] = i_large;
    } else {
      large[large_top++] = i_large;
    }
  }

  while (large_top > 0) {
    u32 idx = large[--large_top];
    alias_method->probability_table[idx] = 1.0f;
    alias_method->alias_table[idx] = idx;
  }
  while (small_top > 0) {
    u32 idx = small[--small_top];
    alias_method->probability_table[idx] = 1.0f;
    alias_method->alias_table[idx] = idx;
  }

  // free scratch if needed
  if (n > SCRATCH_THRESHOLD) {
    free(scratch_buffer);
  }
}

void destroy_alias_method(AliasMethod *alias_method) {
  if (alias_method->dynamic_buffer) {
    free(alias_method->dynamic_buffer);
    alias_method->dynamic_buffer = NULL;
  }
}

u32 draw_alias_method(AliasMethod *alias_method) {
  // 1. generate random x in [0, 1)
  f32 x = random_f32_xoroshiro128_plus(&alias_method->rng);

  for (u32 j = 0; j < alias_method->n; j++) {
    printf("%d ", alias_method->alias_table[j]);
  }
  printf("\n");

  // 2. let i = floor(n * x) and y = nx - i
  //    i uniform in [0, n - 1], y uniform in [0, 1)
  f32 nx = alias_method->n * x;
  u32 i = (u32)nx;
  f32 y = nx - i;

  // 3. if y < U_i, return i
  // else, return K_i
  if (y < alias_method->probability_table[i]) {
    return i;
  }
  return alias_method->alias_table[i];
}

void log_alias_method(const AliasMethod *alias_method) {
  printf("Logging Alias Method\n");

  printf("i | probability[i] | alias[i] \n");
  for (u32 i = 0; i < alias_method->n; i++) {
    printf("%d %f %d\n", i, alias_method->probability_table[i],
           alias_method->alias_table[i]);
  }
}
