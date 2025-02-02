#pragma once

#include <vector>

#include "core/App.h"

#include "FFTCalculator.h"
#include "Generator.h"
#include "Renderer.h"

namespace Waves
{

class WaveApp : public Vision::App
{
public:
  WaveApp();
  ~WaveApp();

  void OnUpdate(float timestep);
  void OnResize(float width, float height);

  void DrawUI();

private:
  std::size_t textureResolution = 256;
  WaveRenderer* waveRenderer = nullptr;
  FFTCalculator* fftCalculator = nullptr;

  std::vector<Generator*> generators;
  bool updateSpectrum = true;

  Vision::ID renderPass = 0;
};

} // namespace Waves