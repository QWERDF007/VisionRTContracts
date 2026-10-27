#pragma once

#include <visionrt/contracts/Value.h>

int contractSharedProbe();
visionrt::DataRef<std::vector<cv::Mat>> contractSharedImages();
void mutateContractSharedImages(const visionrt::DataRef<std::vector<cv::Mat>> &images);
int  contractSharedImageReleases();
visionrt::PortValue                     contractSharedArray();
visionrt::PortValue                     contractSharedRoi();
