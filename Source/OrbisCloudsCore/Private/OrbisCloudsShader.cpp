#include "OrbisCloudsShader.h"

IMPLEMENT_GLOBAL_SHADER(FCloudCoverageMapCS, "/Plugin/OrbisClouds/CloudCoverageMap.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FOrbisCloudsPS, "/Plugin/OrbisClouds/OrbisClouds.usf", "MainPS", SF_Pixel);
