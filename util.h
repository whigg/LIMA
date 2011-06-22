// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__
#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>

#include <iostream>
#include <fstream>
#include <istream>
#include <limits>
#include <string>
#include <vector>

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
#include <vw/Math.h>

using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace std;


void printOverlapList(std::vector<int>  overlapIndices);

//this will be used to compute the makeOverlapList in a more general way.
//it takes into consideration any set of overlapping images.
Vector4 ComputeGeoBoundary(string cubFilename);

//this function determines the image overlap for the general case
//it takes into consideration any set of overlapping images.
std::vector<int> makeOverlapList(std::vector<std::string> inputFiles, Vector4 currCorners);


/* Stream manipulator to ignore until the end of line
 * Use like: std::cin >> ignoreLine;
 */
template <class charT, class traits>
inline
std::basic_istream<charT, traits>&
ignoreLine (std::basic_istream<charT,traits>& stream)
	{
	// skip until end of line
	stream.ignore( std::numeric_limits<int>::max(), stream.widen('\n') );

	return stream;
	}

/* Stream manipulator to ignore one character
 * Use like: std::cin >> ignoreOne;
 */
template <class charT, class traits>
inline
std::basic_istream<charT, traits>&
ignoreOne(std::basic_istream<charT,traits>& stream)
	{
	stream.ignore();

	return stream;
	}

inline Vector3 find_centroid( const vector<Vector3>& points )
  {
  Vector3 centroid;
  for (unsigned int i = 0; i < points.size(); i++)
    {
    centroid += points[i];
    }
  centroid /= points.size();
  return centroid;
  }

#endif

