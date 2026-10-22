#pragma once

#include <visionrt/contracts/DataRef.h>
#include <opencv2/core.hpp>
#include <vector>

int contractSharedProbe();
visionrt::DataRef<std::vector<cv::Mat>> contractSharedImages();
void mutateContractSharedImages(const visionrt::DataRef<std::vector<cv::Mat>> &images);
int  contractSharedImageReleases();
