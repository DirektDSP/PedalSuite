#pragma once

#include "Utils/ParameterSmoother.h"
#include "Core/PneumaticaDSPProcessor.h"

namespace DSP {

using FloatProcessor = Core::PneumaticaDSPProcessor<float>;
using DoubleProcessor = Core::PneumaticaDSPProcessor<double>;

using FloatParameterSmoother = Utils::ParameterSmoother<float>;
using DoubleParameterSmoother = Utils::ParameterSmoother<double>;

} // namespace DSP
