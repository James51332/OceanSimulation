#section common
#version 450 core

#define M_PI 3.1415926545897932384

#define SIZE 8
#define LOG_SIZE 3
#define SHARED_BUFFER_SIZE SIZE
#define NUM_CACHES 2

shared vec2 cache[SHARED_BUFFER_SIZE][NUM_CACHES]; // circular buffer

layout (rgba8, binding = 0) uniform image2D spectrum;
layout (rgba8, binding = 1) uniform image2D normalMap;

// This function takes a final position in the array and computes where it will need to begin for FFT
// Which happens to be the reversed bits of the original
int bitreversal(int value, int bits)
{
  int reversed = 0;
  while (bits > 0)
  {
    reversed <<= 1;
    reversed |= value & 1;
    value >>= 1;

    bits--;
  }

  return reversed;
}

int calculateEvenIndex(int thread, int halfSize)
{
  int dft = thread / halfSize; // integer division
  int firstEven = dft * (halfSize << 1);
  return firstEven + (thread % halfSize);
}

ivec3 iFFT(int thread, int cacheIndex, int evenIndex, int oddIndex)
{
  // Iterately apply butterfly combination until we've done our FFT
  for (int i = 0; i < LOG_SIZE; i++)
  {
    // Wait until all of our workgroup threads are here
    barrier(); 

    // Determine the half size of our tiny DFT
    int halfSize = 1 << i;
    int fullSize = halfSize << 1;
    
    // Find our indices to butterfly
    evenIndex = calculateEvenIndex(thread, halfSize);
    oddIndex = evenIndex + halfSize;

    // Read the values
    vec2 even = cache[cacheIndex][evenIndex];
    vec2 odd = cache[cacheIndex][oddIndex];

    // Butterfly them together using even +/- (twiddle * odd)
    float sampleNum = evenIndex % fullSize;
    float twiddleAngle = (2.0 * M_PI / float(fullSize)) * sampleNum;
    vec2 twiddle = vec2(cos(twiddleAngle), sin(twiddleAngle));

    // (a + bi)(c + di) = (ac - bd) + (ad + bc)i
    odd = vec2(odd.x * twiddle.x - odd.y * twiddle.y, odd.x * twiddle.y + odd.y * twiddle.x);

    // Store in the next ping-pong buffer
    cacheIndex = (cacheIndex + 1) % NUM_CACHES;
    cache[cacheIndex][evenIndex] = even + odd;
    cache[cacheIndex][oddIndex] = even - odd;
  }

  // Return the cache index so that we can copy the data back into the image.
  return ivec3(cacheIndex, evenIndex, oddIndex);
}

#section type(compute) name(horizontalFFT)

layout (local_size_x = SIZE / 2) in;
void main()
{
  ivec2 thread = ivec2(gl_GlobalInvocationID.xy);

  // Copy horizontal data to our cache in bitreversal order.
  int cacheIndex = 0, evenIndex = 2 * thread.x, oddIndex = 2 * thread.x + 1;
  cache[cacheIndex][bitreversal(evenIndex, LOG_SIZE)] = imageLoad(spectrum, ivec2(evenIndex, thread.y)).rg;
  cache[cacheIndex][bitreversal(oddIndex, LOG_SIZE)] =  imageLoad(spectrum, ivec2(oddIndex, thread.y)).rg;

  // Perform the inverse fft and track where data is stored.
  ivec3 indices = iFFT(thread.x, cacheIndex, evenIndex, oddIndex);
  cacheIndex = indices[0];
  evenIndex = indices[1];
  oddIndex = indices[2];

  // Copy our data back into the image multiplied by scaling
  imageStore(spectrum, ivec2(evenIndex, thread.y), vec4(cache[cacheIndex][evenIndex] * (1.0 / SIZE), 0.0, 1.0));
  imageStore(spectrum, ivec2(oddIndex, thread.y), vec4(cache[cacheIndex][oddIndex] * (1.0 / SIZE), 0.0, 1.0));
}

#section type(compute) name(verticalFFT)

layout (local_size_y = SIZE / 2) in;
void main()
{
  ivec2 thread = ivec2(gl_GlobalInvocationID.xy);

  // Copy vertical data to our cache in bitreversal order
  int cacheIndex = 0, evenIndex = 2 * thread.y, oddIndex = 2 * thread.y + 1;
  cache[cacheIndex][bitreversal(evenIndex, LOG_SIZE)] = imageLoad(spectrum, ivec2(thread.x, evenIndex)).rg;
  cache[cacheIndex][bitreversal(oddIndex, LOG_SIZE)] =  imageLoad(spectrum, ivec2(thread.x, oddIndex)).rg;

  // Perform the inverse fft and track where data is stored
  ivec3 indices = iFFT(thread.y, cacheIndex, evenIndex, oddIndex);
  cacheIndex = indices[0];
  evenIndex = indices[1];
  oddIndex = indices[2];

  // Copy our data back into the image multiplied by scaling
  imageStore(spectrum, ivec2(thread.x, evenIndex), vec4(cache[cacheIndex][evenIndex] * (1.0 / SIZE), 0.0, 1.0));
  imageStore(spectrum, ivec2(thread.x, oddIndex), vec4(cache[cacheIndex][oddIndex] * (1.0 / SIZE), 0.0, 1.0));
}