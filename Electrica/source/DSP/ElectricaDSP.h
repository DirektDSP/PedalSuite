#pragma once

#include "Utils/ParameterSmoother.h"
#include "Core/ElectricaDSPProcessor.h"

namespace DSP {

using FloatProcessor = Core::ElectricaDSPProcessor<float>;
using DoubleProcessor = Core::ElectricaDSPProcessor<double>;

using FloatParameterSmoother = Utils::ParameterSmoother<float>;
using DoubleParameterSmoother = Utils::ParameterSmoother<double>;

} // namespace DSP
