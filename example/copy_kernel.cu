#include "copy_kernel.h"

// Vectorized copy kernel: each thread copies one uint4 (16 bytes). This keeps
// the SMs issuing wide load/store transactions over NVLink peer access, which
// is what we want when comparing kernel-based traffic against cudaMemcpy.
__global__ void copyKernel(uint4* dst, const uint4* src, size_t count) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < count) {
        dst[idx] = src[idx];
    }
}

cudaError_t launchCopyKernel(void* dst, const void* src, size_t size_bytes,
                             int device, cudaStream_t stream) {
    cudaError_t err = cudaSetDevice(device);
    if (err != cudaSuccess) {
        return err;
    }

    // uint4 is 16 bytes; size_bytes is always a multiple of 16 (see header).
    size_t count = size_bytes / sizeof(uint4);
    const int block = 256;
    int grid = static_cast<int>((count + block - 1) / block);

    copyKernel<<<grid, block, 0, stream>>>(reinterpret_cast<uint4*>(dst),
                                           reinterpret_cast<const uint4*>(src),
                                           count);
    return cudaGetLastError();
}
