#section type(compute) name(fft)
#version 450 core

#define SIZE 8
#define LOG_SIZE int(log2(SIZE))
#define NUM_CACHES 2
#define M_PI 3.1415926535897932384

shared vec2 cache[SIZE][NUM_CACHES];

layout (binding = 0) buffer ssbo
{
    vec2 data[SIZE];
};

uint reverseBits(uint num, uint numBits)
{
    uint value = 0;

    for (uint i = 0; i < numBits; i++)
    {
        value <<= 1;
        value |= (num & 1);
        num >>= 1;
    }

    return value;
}

uvec3 fft(uint thread, uint cacheIndex, uint evenIndex, uint oddIndex)
{
    for (int i = 0; i < LOG_SIZE; i++) // each iteration represents a combination of stages
    {
        // ensure that all threads have finished the last iteration
        barrier(); 

        // calculate our new even and odd indices
        // 1) use one thread per set of even and odd indices
        // 2) each dft requires half its size worth of threads => dft # is (thread div halfSize)
        // 3) even index = dftNum * dftSize + (thread mod halfSize)
        // 4) odd index = evenIndex + halfSize
        uint halfSize = 1u << i;
        uint fullSize = halfSize << 1;
        int dftNum = int(thread) / int(halfSize);
        int dftElement = int(thread) % int(halfSize);
        evenIndex = dftNum * int(fullSize) + dftElement;
        oddIndex = evenIndex + int(halfSize);

        // compute the twiddle factor for combining two dfts
        float twiddleAngle = -2.0 * M_PI * dftElement / fullSize; // making this positive causes iFFT
        vec2 twiddle = vec2(cos(twiddleAngle), sin(twiddleAngle));

        // retrieve our values and twiddle the odd factor
        vec2 even = cache[cacheIndex][evenIndex];
        vec2 odd = cache[cacheIndex][oddIndex];

        odd = vec2(odd.x * twiddle.x - odd.y * twiddle.y,
                   odd.x * twiddle.y + odd.y * twiddle.x);
                
        // move to the next ping pong buffer and combine
        cacheIndex = (cacheIndex + 1) % NUM_CACHES;
        cache[cacheIndex][evenIndex] = even + odd;
        cache[cacheIndex][oddIndex] = even - odd;
    }

    return uvec3(cacheIndex, evenIndex, oddIndex);
}

layout (local_size_x = SIZE/2) in;
void main()
{
    // each thread is responsible for one even index and one odd index
    uint thread = uint(gl_LocalInvocationID.x);
    uint cacheIndex = 0;
    uint evenIndex = thread * 2;
    uint oddIndex = evenIndex + 1;

    // copy from our buffer to the cache in bit reversal order
    cache[cacheIndex][evenIndex] = data[reverseBits(evenIndex, LOG_SIZE)];
    cache[cacheIndex][oddIndex] = data[reverseBits(oddIndex, LOG_SIZE)];
    
    // perform the fft
    uvec3 indices = fft(thread, cacheIndex, evenIndex, oddIndex);
    cacheIndex = indices.x;
    evenIndex = indices.y;
    oddIndex = indices.z;

    // copy our data back into the ssbo
    data[evenIndex] = cache[cacheIndex][evenIndex];
    data[oddIndex] = cache[cacheIndex][oddIndex];
}