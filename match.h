// __BEGIN_LICENSE__
// Copyright (C) 2006, 2007 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#ifndef MATCH_H
#define MATCH_H

#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/operators.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;

#include <vw/Core.h>
#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/Cartography.h>
#include <vw/Photometry.h>
#include <vw/Math.h>
#include <vw/Math/Matrix.h>
using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace vw::photometry;

using namespace std;
#include <math.h>
//#include <cv.h>
//#include <highgui.h>

#include "coregister.h"

float ComputeScaleFactor(vector<float> allImgPts, vector<float> reflectance);
float ComputeScaleFactor(vector<Vector3> allImgPts, vector<float> reflectance);
vector<float> ComputeSyntImgPts(float scaleFactor, vector<float> reflectance);
Vector<float,6> UpdateMatchingParams(vector<vector<LOLAShot> > trackPts, string DRGFilename, ModelParams modelParams,GlobalParams globalParams, bool other_d, vector<float> d2);
float ComputeMatchingError(vector<float> reflectancePts, vector<float>imgPts);

#endif//MATCH_H
