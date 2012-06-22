/**
 * This program takes as input a cub file which is an image of a planetary surface,
 * and a CSV file of LOLA lidar points. It performs a brute force search over a
 * local area to find the transform which best fits the lidar points to the image
 * according to their estimate reflectance.
 **/

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <sstream>

#include <boost/operators.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#include <vw/Core.h>
#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/Cartography.h>
#include <vw/Math.h>
#include <vw/Math/Matrix.h>
#include <asp/IsisIO.h>
#include <asp/IsisIO/IsisCameraModel.h>

using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace vw::camera;
using namespace std;

#include <math.h>
#include "util.h"
#include "tracks.h"
#include "match.h"
#include "coregister.h"
#include "display.h"
#include "weights.h"
#include "featuresLOLA.h"
	
int main( int argc, char *argv[] )
{
	// command line options
	string inputCSVFilename; 
	
	std::string resDir = "../results";
	std::string inputCubFile;
	std::string outputFile, dataFile, imageFile, startMatrix;
	Matrix3x3 matrix(1, 0, 0, 0, 1, 0, 0, 0, 1);
	int transSearchWindow = 20, transSearchStep = 5;
	float thetaSearchWindow = (M_PI / 10), thetaSearchStep = (M_PI / 40);

	po::options_description general_options("Options");
	general_options.add_options()
	("Lidar-filename,l", po::value<std::string>(&inputCSVFilename))
	("inputCubFile,i", po::value<std::string>(&inputCubFile))
	("outputFile,o", po::value<std::string>(&outputFile))
	("dataFile,d", po::value<std::string>(&dataFile))
	("outputImage", po::value<std::string>(&imageFile))
	("startMatrix,m", po::value<std::string>(&startMatrix))
	("transSearchWindow", po::value<int>(&transSearchWindow))
	("transSearchStep", po::value<int>(&transSearchStep))
	("thetaSearchWindow", po::value<float>(&thetaSearchWindow))
	("thetaSearchStep", po::value<float>(&thetaSearchStep))
	("results-directory,r", po::value<std::string>(&resDir)->default_value("../results"), "results directory.")
	("help,h", "Display this help message");
	
	po::options_description options("Allowed Options");
	options.add(general_options);

	po::positional_options_description p;
	p.add("inputCubFile", -1);

	std::ostringstream usage;
	usage << "Description: main code for Lidar to image co-registration" << std::endl << std::endl;
	usage << general_options << std::endl;
	
	po::variables_map vm;
	try
	{
		po::store( po::command_line_parser( argc, argv ).options(options).positional(p).run(), vm );
		po::notify( vm );
	}
	catch ( po::error &e )
	{
		std::cout << "An error occured while parsing command line arguments.\n";
		std::cout << "\t" << e.what() << "\n\n";
		std::cout << usage.str();
		return 1;
	}

	if (vm.count("startMatrix"))
	{
		int ret = sscanf(startMatrix.c_str(), "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", 
			&matrix[0][0], &matrix[0][1], &matrix[0][2], 
			&matrix[1][0], &matrix[1][1], &matrix[1][2], 
			&matrix[2][0], &matrix[2][1], &matrix[2][2]);
		if (ret != 9)
		{
			fprintf(stderr, "Failed to read startMatrix argument.\n");
			return 1;
		}
	}

	if( vm.count("help") )
	{
		std::cerr << usage.str() << std::endl;
		return 1;
	}

	if(( vm.count("inputCubFile") < 1 ))
	{
		std::cerr << "Error: Must specify a cub image file!" << std::endl << std::endl;
		std::cerr << usage.str();
		return 1;
	}
	
	//create the results directory and prepare the output filenames - START
	string makeResDirCmd = "mkdir -p " + resDir;
	int ret = system(makeResDirCmd.c_str()); 

	if (ret)
	{
		fprintf(stderr, "Failed to create directory.\n");
		return 1;
	}


	// done parsing arguments

	vector<vector<LOLAShot> > trackPts =	CSVFileRead(inputCSVFilename);
	vector<gcp> gcpArray = ComputeSalientLOLAFeatures(trackPts);
	
	camera::IsisCameraModel model(inputCubFile);
	Vector3 center_of_moon(0,0,0);
	Vector2 pixel_location = model.point_to_pixel( center_of_moon );
	Vector3 cameraPosition = model.camera_center( pixel_location );
	Vector3 lightPosition = model.sun_position( pixel_location );
	
	//initialization step for LIMA - START	
	GetAllPtsFromCub(trackPts, inputCubFile);
	
	ComputeAllReflectance(trackPts, cameraPosition, lightPosition);
	vector<vector< AlignedLOLAShot> > aligned = initialize_aligned_lola_shots(trackPts);
	transform_tracks(aligned, matrix, inputCubFile);
	
	//find_track_transforms(aligned, inputCubFile);
	Matrix3x3 trans = find_tracks_transform(aligned, inputCubFile, matrix, 
			transSearchWindow, transSearchStep, thetaSearchWindow, thetaSearchStep);

	FILE* output = stdout;
	if (outputFile.length() > 0)
	{
		output = fopen(outputFile.c_str(), "w");
		if (output == NULL)
		{
			fprintf(stderr, "Failed to open output file %s.\n", outputFile.c_str());
			output = stdout;
		}
	}
	for (int i = 0; i < 3; i++)
		fprintf(output, "%g %g %g\n", trans(i, 0), trans(i, 1), trans(i, 2));
	fclose(output);

	if (dataFile.length() > 0)
		save_track_data(aligned, dataFile);
	if (imageFile.length() > 0)
		SaveReflectanceImages(aligned, inputCubFile, imageFile, true);
 
	return 0;

}

