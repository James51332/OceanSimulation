#section common
#version 450 core

#define SIZE 256
#define LOG_SIZE int(log2(SIZE))
#define NUM_CACHES 2
#define M_PI 3.1415926535897932384

shared vec2 cache[SIZE][NUM_CACHES];

layout (rgba32f, binding = 0) uniform image2D image;

uint reverseBits(uint num, uint numBits)
{
  return (bitfieldReverse(num) >> (32 - numBits));
}

uvec3 ifft(uint thread, uint cacheIndex, uint evenIndex, uint oddIndex)
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
    float twiddleAngle = 2.0 * M_PI * dftElement / fullSize; // making this positive causes iFFT
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

#section type(compute) name(horizontalIFFT)

layout (local_size_x = SIZE/2) in;
void main()
{
  // each thread is responsible for one even index and one odd index
  uint thread = uint(gl_LocalInvocationID.x);
  uint cacheIndex = 0;
  uint evenIndex = thread * 2;
  uint oddIndex = evenIndex + 1;

  // center lowest amplitudes at center (0 => SIZE) -> (SIZE/2 => SIZE; 0 => SIZE/2)
  uint evenRead = (evenIndex + SIZE/2) % SIZE; 
  uint oddRead = (oddIndex + SIZE/2) % SIZE;
    
  // copy from our buffer to the cache in bit reversal order
  cache[cacheIndex][reverseBits(evenIndex, LOG_SIZE)] = imageLoad(image, ivec2(evenRead, gl_GlobalInvocationID.y)).xy;
  cache[cacheIndex][reverseBits(oddIndex, LOG_SIZE)] = imageLoad(image, ivec2(oddRead, gl_GlobalInvocationID.y)).xy;
    
  // perform the fft
  uvec3 indices = ifft(thread, cacheIndex, evenIndex, oddIndex);
  cacheIndex = indices.x;
  evenIndex = indices.y;
  oddIndex = indices.z;

  // copy our data back into the ssbo
  imageStore(image, ivec2(evenIndex, gl_GlobalInvocationID.y), vec4(cache[cacheIndex][evenIndex], 0.0, 1.0));
  imageStore(image, ivec2(oddIndex, gl_GlobalInvocationID.y), vec4(cache[cacheIndex][oddIndex], 0.0, 1.0));
}

#section type(compute) name(verticalIFFT)

layout (local_size_y = SIZE/2) in;
void main()
{
  // each thread is responsible for one even index and one odd index
  uint thread = uint(gl_LocalInvocationID.y);
  uint cacheIndex = 0;
  uint evenIndex = thread * 2;
  uint oddIndex = evenIndex + 1;

  // center lowest amplitudes at center (0 => SIZE) -> (SIZE/2 => 0; SIZE => SIZE/2)
  uint evenRead = (SIZE + SIZE/2 - 1 - evenIndex) % SIZE; 
  uint oddRead = (SIZE + SIZE/2 - 1 - oddIndex) % SIZE;

  // copy from our buffer to the cache in bit reversal order
  cache[cacheIndex][reverseBits(evenIndex, LOG_SIZE)] = imageLoad(image, ivec2(gl_GlobalInvocationID.x, evenRead)).xy;
  cache[cacheIndex][reverseBits(oddIndex, LOG_SIZE)] = imageLoad(image, ivec2(gl_GlobalInvocationID.x, oddRead)).xy;

  // perform the fft
  uvec3 indices = ifft(thread, cacheIndex, evenIndex, oddIndex);
  cacheIndex = indices.x;
  evenIndex = indices.y;
  oddIndex = indices.z;

  // copy our data back into the ssbo
  imageStore(image, ivec2(gl_GlobalInvocationID.x, evenIndex), vec4(cache[cacheIndex][evenIndex], 0.0, 1.0));
  imageStore(image, ivec2(gl_GlobalInvocationID.x, oddIndex), vec4(cache[cacheIndex][oddIndex], 0.0, 1.0));
}

#section type(compute) name(generateSpectrum)

void main()
{
  ivec2 thread = ivec2(gl_GlobalInvocationID.xy);
  vec2 frequency = vec2(thread.x - SIZE/2, SIZE/2 - 1 - thread.y);
  vec2 uv = vec2(frequency) * (2.0 / SIZE);
  float dist = 1.0 - min(length(uv), 1.0);

  vec4 color = vec4(0.0, 0.0, 0.0, 1.0);

  if (frequency.y == 1 && frequency.x == 2)
    color.r = 1.0;

  imageStore(image, thread, color);
}