#pragma once

#include "Utils/ParameterSmoother.h"
#include "Core/HydraulicaDSPProcessor.h"

namespace DSP {

using FloatProcessor = Core::HydraulicaDSPProcessor<float>;
using DoubleProcessor = Core::HydraulicaDSPProcessor<double>;

using FloatParameterSmoother = Utils::ParameterSmoother<float>;
using DoubleParameterSmoother = Utils::ParameterSmoother<double>;

} // namespace DSP
