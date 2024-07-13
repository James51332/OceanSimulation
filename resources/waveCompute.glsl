#type compute
#version 450 core

// This function takes a final position in the array and computes where it will need to begin for FFT
int bitreversal(int index, int size)
{
  int lower = 0; // track first element of dft

  // non-recursive implementation
  while (size > 2)
  {
    int offset = index - lower;
    int odd = offset % 2; // odd => 1, even => 0

    // bitwise division by two (power of two)
    size >>= 1;

    // if even, lower stays same. otherwise, lower increases by size of next dft
    lower += size * odd; // branchless math

    // if even, divide offset by two. if odd, subtract one from offset, divide by two
    index = lower + ((offset - odd) >> 1); // lower + new offset
  }

  return index;
}

#define M_PI 3.1415926545897932384

#define SIZE 8
#define LOG_SIZE 3
#define SHARED_BUFFER_SIZE SIZE

layout (local_size_x = SIZE / 2) in;

#define NUM_CACHES 2
shared vec2 cache[SHARED_BUFFER_SIZE][NUM_CACHES]; // circular buffer

layout (std430, binding = 0) buffer ssbo
{
  vec2 data[SIZE];
};

int calculateEvenIndex(int thread, int halfSize)
{
  int dft = thread / halfSize; // integer division
  int firstEven = dft * halfSize * 2;
  return firstEven + (thread % halfSize);
}

void main()
{
  int n = int(gl_GlobalInvocationID.x);
  int evenIndex = n*2, oddIndex = n*2+1;

  // Copy to our cache in bitreversal order
  int cacheIndex = 0;
  cache[cacheIndex][evenIndex] = data[bitreversal(evenIndex, SIZE)];
  cache[cacheIndex][oddIndex] = data[bitreversal(oddIndex, SIZE)];

  // Iterately apply butterfly combination until we've done our FFT
  for (int i = 0; i < LOG_SIZE; i++)
  {
    // Wait until all of our workgroup threads are here
    barrier(); 

    // Determine the half size of our tiny DFT
    int halfSize = int(1u << i);
    int fullSize = halfSize * 2;
    
    // Find our indices to butterfly
    evenIndex = calculateEvenIndex(n, halfSize);
    oddIndex = evenIndex + halfSize;

    // Read the values
    vec2 even = cache[cacheIndex][evenIndex];
    vec2 odd = cache[cacheIndex][oddIndex];

    // Butterfly them together
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

  // Copy our data back into the buffer multiplied by scaling
  data[evenIndex] = cache[cacheIndex][evenIndex] * (1.0 / SIZE);
  data[oddIndex] = cache[cacheIndex][oddIndex] * (1.0 / SIZE);
}