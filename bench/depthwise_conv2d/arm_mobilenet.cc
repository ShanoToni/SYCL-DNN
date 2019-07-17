/*
 * Copyright 2019 Codeplay Software Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if defined(ARM_COMPUTE)
#include "arm_fixture.h"
// For ARM Compute Library, need to provide the executor to specify whether
// running on NEON or OpenCL.
#ifdef ACL_NEON
#define EXEC sycldnn::bench::ACLNeonExecutor
#else
#define EXEC sycldnn::bench::ACLOpenCLExecutor
#endif  // ACL_NEON

#else
#error Cannot compile without ARM_COMPUTE defined
#endif

#include "param_set.h"

#define MOBILENET_BENCHMARK_WITH_DTYPE(N, WIN, STR, H, W, C, MUL, PAD, DTYPE) \
  DEPTHWISE_CONVOLUTION_BENCHMARK(                                            \
      "MobileNet", Forward_##N##_##WIN##_##STR##_##H##_##W##_##C, DTYPE,      \
      ParameterSet<N, WIN, STR, H, W, C, MUL, PAD>, EXEC)

#define MOBILENET_BENCHMARK(N, WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK_WITH_DTYPE(N, WIN, STR, H, W, C, MUL, PAD, float)

// Standard benchmark sizes (batch size: 1, 4, optionally 32
#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(1, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS

#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(4, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS

#ifdef SNN_LARGE_BATCH_BENCHMARKS
#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(32, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS
#endif  // SNN_LARGE_BATCH_BENCHMARKS

// Extended benchmarks (batch size: 2, optionally 8, 16, 64)
#ifdef SNN_EXTENDED_BENCHMARKS
#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(2, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS

#ifdef SNN_LARGE_BATCH_BENCHMARKS
#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(8, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS

#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(16, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS

#define MOBILENET_PARAMS(WIN, STR, H, W, C, MUL, PAD) \
  MOBILENET_BENCHMARK(64, WIN, STR, H, W, C, MUL, PAD);
#include "bench/depthwise_conv2d/mobilenet_params.def"
#undef MOBILENET_PARAMS
#endif  // SNN_LARGE_BATCH_BENCHMARKS
#endif  // SNN_EXTENDED_BENCHMARKS