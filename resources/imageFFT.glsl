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

  // center lowest amplitudes at center (0 => SIZE - 1) -> (SIZE/2 + 1 => 0; 1 => SIZE/2)
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
  uint evenRead = (evenIndex + SIZE/2) % SIZE;
  uint oddRead = (oddIndex + SIZE/2) % SIZE;

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

layout (rgba32f, binding = 1) uniform image2D gaussianImage;

float phillips(vec2 waveNumber, vec2 windVelocity, float gravity)
{
  float waveNumberLength = length(waveNumber);
  if (waveNumberLength == 0.0)
    return 0.0;

  float waveNumberLength2 = waveNumberLength * waveNumberLength;
  float waveNumberLength4 = waveNumberLength2 * waveNumberLength2;
  vec2 waveNumberDir = normalize(waveNumber);
  
  float windSpeed = length(windVelocity);
  vec2 windDir = normalize(windVelocity);
  
  float waveDotWind = dot(waveNumberDir, windDir);
  float waveDotWind2 = waveDotWind * waveDotWind;
  
  float L = windSpeed * windSpeed / gravity;
  float L2 = L * L;
  
  return exp(-1.0 / (waveNumberLength2 * L2)) / waveNumberLength4 * waveDotWind2;
}

float dispersion(vec2 waveNumber, float gravity)
{
  return sqrt(gravity * length(waveNumber));
}

// layout (std140, binding = 0) uniform settings
// {
//   float time;
//   vec3 dummy;
// };

void main()
{
  float gravity = 9.8;
  float scale = 0.001;
  vec2 windVelocity = vec2(8.0, 2.0);

  vec2 thread = vec2(gl_GlobalInvocationID.xy);
  vec2 frequency = ((thread / SIZE) - 0.5) * 2.0;
  vec2 waveNumber = frequency * M_PI;

  // get a gaussian random value
  vec2 currentValue = imageLoad(gaussianImage, ivec2(thread)).xy;

  // determine the phase based on the time and rotate the gaussian
  // float phase = time * dispersion(waveNumber, gravity);
  // vec2 rotate = vec2(cos(phase), sin(phase));

  // currentValue = vec2(currentValue.x * rotate.x - currentValue.y * rotate.y,
  //                     currentValue.x * rotate.y + currentValue.y * rotate.x);
  
  // calculate the amplitude scale using the phillips spectrum
  vec2 amp = scale * currentValue * sqrt(phillips(waveNumber, windVelocity, gravity));
  
  vec4 color = vec4(amp, 0.0, 1.0);
  imageStore(image, ivec2(thread), color);
}
