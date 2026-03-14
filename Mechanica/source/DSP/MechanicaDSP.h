#pragma once

#include "Utils/ParameterSmoother.h"
#include "Core/MechanicaDSPProcessor.h"

namespace DSP {

using FloatProcessor = Core::MechanicaDSPProcessor<float>;
using DoubleProcessor = Core::MechanicaDSPProcessor<double>;

using FloatParameterSmoother = Utils::ParameterSmoother<float>;
using DoubleParameterSmoother = Utils::ParameterSmoother<double>;

} // namespace DSP
