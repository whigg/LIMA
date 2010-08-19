// __BEGIN_LICENSE__
// Copyright (C) 2006, 2007 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#ifndef DISPLAY_H
#define DISPLAY_H

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
using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace vw::photometry;

using namespace std;
#include <math.h>
void ShowFinalTrackPtsOnImage(vector<vector<LOLAShot> >trackPts, Vector<float, 6> d, 
                              vector<int> trackIndices, string DRGFilename, string outFilename);
void MakeGrid(vector<vector<LOLAShot> >trackPts, int numVerPts, int numHorPts, string DEMFilename, vector<int> trackIndices);
#endif /* DISPLAY_H */
