#ifndef NVLINK_BW_TEST_COPY_KERNEL_H
#define NVLINK_BW_TEST_COPY_KERNEL_H

#include <cuda_runtime.h>

#include <cstddef>

// Launches a vectorized (uint4, 16-byte per thread) copy kernel on `device`.
// Caller is responsible for timing (e.g. cudaEvent) and synchronization.
//
// size_bytes must be a multiple of 16 (buffer_size_mb * 1024 * 1024 always is,
// since 1 MiB is divisible by 16). cudaMalloc returns 256-byte-aligned memory,
// which satisfies uint4's 16-byte alignment requirement.
//
// Returns cudaSuccess on successful launch. Launch-configuration errors are
// captured via cudaGetLastError before returning; execution errors (e.g.
// illegal memory access) surface at the caller's next synchronization point.
//
// `stream` selects the launch stream (0 = default stream). Used by the
// bidirectional test to run two kernels concurrently on separate GPUs/streams.
cudaError_t launchCopyKernel(void* dst, const void* src, size_t size_bytes,
                             int device, cudaStream_t stream = 0);

#endif  // NVLINK_BW_TEST_COPY_KERNEL_H
