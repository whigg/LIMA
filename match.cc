// __BEGIN_LICENSE__
// Copyright (C) 2006, 2007 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#include <omp.h>
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

#include <stdio.h>

using namespace vw;
using namespace vw::math;
using namespace vw::cartography;
using namespace vw::photometry;

using namespace std;
#include <math.h>
#include "io.h"
#include "coregister.h"
#include "tracks.h"
#include "weights.h"


#define CHUNKSIZE   1


void SaveMatchResults(vector<Vector<float, 6> >finalTransfArray,  vector<float> errorArray, string matchResultsFilename)
{
    FILE *d_FILE = fopen(matchResultsFilename.c_str(),"w");
    int numElements = errorArray.size();
    for (int i = 0; i < numElements; i++){
       fprintf(d_FILE,"index=%d d[0]= %f d[1]= %f d[2]= %f d[3]= %f d[4]= %f d[5]= %f e=%f\n", 
	               i,
                       finalTransfArray[i](0), finalTransfArray[i](1), 
                       finalTransfArray[i](2), finalTransfArray[i](3),
                       finalTransfArray[i](4), finalTransfArray[i](5),
	               errorArray[i]);
    }
    fclose(d_FILE);
}
/*
bool deriv_cached(string & input_name, string & cached_table){
  bool identified = false;
  string line;
  fstream myfile(cached_table.c_str());
  // does the name exist
  if(myfile.is_open()){
    while(! myfile.eof()){
      getline(myfile,line);
      cout << line << endl;
      if(line == input_name){
        identified = true;
        cout << "Fount it!" << endl; 
      }
    }
  }
  myfile.close();

  //if not add the name to the cached file
  if(!identified){
    // if we didn't find the file append the file to the document of saves
    ofstream file_change;
    file_change.open(cached_table.c_str(),ios::out | ios:: app);
    file_change << input_name << "\n";
    file_change.close();
  }


  return identified;
}
*/
void printRHS( Matrix<float,6,6> rhs,  int index, int iter)
{
  cout<<"index= "<<index<<", iter= "<<iter<<endl;
  printf("--------------------------------------\n");
  printf("RHS = \n");
  printf("[ %f %f %f | %f %f %f]\n",rhs(0,0), rhs(0,1), rhs(0,2), rhs(0,3), rhs(0,4), rhs(0,5) );   
  printf("[ %f %f %f | %f %f %f]\n", rhs(1,0), rhs(1,1), rhs(1,2), rhs(1,3), rhs(1,4),rhs(1,5) ); 
  printf("[ %f %f %f | %f %f %f]\n", rhs(2,0), rhs(2,1),rhs(2,2), rhs(2,3), rhs(2,4), rhs(2,5) );
  printf("[ %f %f %f | %f %f %f]\n", rhs(3,0), rhs(3,1), rhs(3,2), rhs(3,3), rhs(3,4), rhs(3,5) );
  printf("[ %f %f %f | %f %f %f]\n", rhs(4,0), rhs(4,1), rhs(4,2), rhs(4,3), rhs(4,4), rhs(4,5) );
  printf("[ %f %f %f | %f %f %f]\n", rhs(5,0), rhs(5,1), rhs(5,2), rhs(5,3), rhs(5,4), rhs(5,5) );
  printf("--------------------------------------\n\n");
}

void printLHS_Error( Vector<float,6> lhs, float error, int index, int iter)
{
 
  cout<<"index= "<<index<<", iter= "<<iter<<", error="<<error<<endl;
  printf("--------------------------------------\n");
  printf("LHS = [ %f, %f, %f, %f, %f, %f]\n", lhs[0], lhs[1], lhs[2], lhs[3], lhs[4], lhs[5]);
  printf("--------------------------------------\n\n");
}


void UpdateMatchingParams(vector<vector<LOLAShot> > &trackPts, string DRGFilename,  
			  ModelParams modelParams,  int numMaxIter, 
			  vector<Vector<float, 6> >initTransfArray, vector<Vector<float, 6> >&finalTransfArray, 
			  vector<float> &errorArray )
{

  DiskImageView<PixelGray<uint8> >   DRG(DRGFilename);
  GeoReference DRGGeo;
  read_georeference(DRGGeo, DRGFilename);

  cout <<"Interpolating the image ...";
  ImageViewRef<PixelGray<uint8> >   interpDRG = interpolate(edge_extend(DRG.impl(),
        ConstantEdgeExtension()),
      BilinearInterpolation());
  cout<<"done."<<endl;


  cout << "Computing the scale factor...";
  float scaleFactor;
  scaleFactor = ComputeScaleFactor(trackPts);
  cout<<"done."<<endl;

  cout << "Computing/Reading the derivative image..."; 
  //to be used in the RELEASE!- DO NOT remove it!
  //DiskCacheImageView<float> x_deriv = derivative_filter(DRG,1,0);
  //DiskCacheImageView<float> y_deriv = derivative_filter(DRG,0,1);

  std::string temp = sufix_from_filename(DRGFilename);

  std::string xDerivFilename = "../results" + prefix_less3_from_filename(temp) + "_x_deriv.tif";
  std::string yDerivFilename = "../results" + prefix_less3_from_filename(temp) + "_y_deriv.tif";

  if ( !boost::filesystem::exists( xDerivFilename ) ) {
    cout << "Computing the x_derivative ..." << endl;
    ImageViewRef<float> temp = derivative_filter(pixel_cast<float>(DRG),1,0);
    DiskImageResourceGDAL rsrc( xDerivFilename,
        temp.format(), Vector2i(512,512) );
    cout << "Writing the x_derivative" << endl;
    block_write_image(rsrc, temp,
        TerminalProgressCallback("asp", "Derivative:") );
    cin.get();
    cout<<"done."<<endl;
  }
  cout << "Reading in the x_deriv from "<<xDerivFilename<<" ..." << endl;

  DiskImageView<float> x_deriv( xDerivFilename );

  if ( !boost::filesystem::exists( yDerivFilename ) ) {
     cout << "Computing the y_derivative ..." << endl;

    ImageViewRef<float> temp = derivative_filter(pixel_cast<float>(DRG),0,1);
    DiskImageResourceGDAL rsrc( yDerivFilename,
        temp.format(), Vector2i(512,512) );
    cout << "Writing the y_derivative" << endl;
    block_write_image(rsrc, temp,
        TerminalProgressCallback("asp", "Derivative:") );
  }
 
  cout << "Reading in the y_deriv from "<<yDerivFilename<<" ... "<< endl;
  DiskImageView<float> y_deriv( yDerivFilename );
  cout<<"done."<<endl;
  

  //float xx,yy;
  int row_max, col_max;

  row_max = x_deriv.rows();
  col_max = x_deriv.cols();
   
  cout << "image size: rows: " << row_max << ", cols:"<< col_max << endl;
  int i_C = row_max/2;
  int j_C = col_max/2;

  //working with the entire CVS file significant errors where seen at index 126 - set index to this by hand to debugg this test case
 /*
  1. we see huge problems from the get go - massive lhs numbers, where does these come from in comparison
  2. need to explore & instrument the code further
  3. we have some stable/unstable pairs to explore (106 stable, 109 not) & ( 112 stable, 115 not)
 
  A couple of thoughts - we rezero lhs & rhs every iteration this should not be the problem.  Could the problem simple be a scaling issue where the size of the error terms on the right & left hand side - caused by the great number of points - causes numerical stability issues for solving the matrix?
*/

  //copy initTransfArray into finalTransfArray.
  for (int index = 0; index < initTransfArray.size(); index++){
    for (int i = 0; i < 6; i++){
      finalTransfArray[index][i]=initTransfArray[index][i];
    }
  }

  //for (int index = 0; index < initTransfArray.size(); index++){
  //for (int index = 112; index < 117; index++){
  for (int index = 125; index < 128; index++){ 
    cout << "index = "<< index << endl;
  
  
    int ti,si,li;//indices for tracks, shots and lola points respectively
    int iA, jA;
    int ii, jj;
    int iter;
    float I_x_sqr, I_x_I_y, I_y_sqr; 
    float I_y_val, I_x_val;
    float I_e_val ;
    Matrix<float,6,6> rhs;
    Vector<float,6> lhs;
 
    iA = 0;
    jA = 0;
    iter = 0;

    while( iter <= numMaxIter){ //gradient descent => optimal transform

      //reset rhs & lhs
      for(int i_RHS = 0; i_RHS < 6; i_RHS++){
        for(int j_RHS = 0; j_RHS < 6; j_RHS++){
          rhs(i_RHS,j_RHS) = 0.0;
        }
        lhs(i_RHS) = 0.0;
      }
 
      //reset the error;
      errorArray[index] = 0.0;

      for (ti = 0; ti < trackPts.size(); ti++){

        for (si = 0; si < trackPts[ti].size(); si++){

          if ((trackPts[ti][si].valid ==1) && (trackPts[ti][si].reflectance !=0)){
           
            //weight = trackPts[k][i].weight_prd;
        
            for (li = 0; li < trackPts[ti][si].LOLAPt.size(); li++){//for each point of a LOLA shot

              if (trackPts[ti][si].LOLAPt[li].s == 3){//center point of a valid shot
    
		iA = (int) floor(finalTransfArray[index][0]*trackPts[ti][si].imgPt[li].x + finalTransfArray[index][1]*trackPts[ti][si].imgPt[li].y + finalTransfArray[index][2]);
		jA = (int) floor(finalTransfArray[index][3]*trackPts[ti][si].imgPt[li].x + finalTransfArray[index][4]*trackPts[ti][si].imgPt[li].y + finalTransfArray[index][5]);
                
                // check (iA,jA) are inside the image!
		if ( ( iA >= 0) && ( iA < row_max) && ( jA >= 0) && ( jA < col_max)){
        	  
                  // calculate ii & jj relative to the image center
		  ii = iA - i_C;
		  jj = jA - j_C;

		  I_e_val = interpDRG(jA,iA) - scaleFactor*trackPts[ti][si].reflectance;
		  errorArray[index] += abs(I_e_val);

		  //calculate numerical dirivatives (ii,jj).
		  I_x_val = x_deriv(jA,iA); 
		  I_y_val = y_deriv(jA,iA); 

		  I_x_I_y = I_x_val*I_y_val;        
		  I_x_sqr = I_x_val*I_x_val;
		  I_y_sqr = I_y_val*I_y_val;

		  // Left hand side
		  lhs(0) += ii * I_x_val * I_e_val;
		  lhs(1) += jj * I_x_val * I_e_val;
		  lhs(2) +=      I_x_val * I_e_val;
		  lhs(3) += ii * I_y_val * I_e_val;
		  lhs(4) += jj * I_y_val * I_e_val;
		  lhs(5) +=      I_y_val * I_e_val;

		  // Right Hand Side UL
		  rhs(0,0) += ii*ii * I_x_sqr;
		  rhs(0,1) += ii*jj * I_x_sqr;
		  rhs(0,2) += ii    * I_x_sqr;
		  rhs(1,1) += jj*jj * I_x_sqr;
		  rhs(1,2) += jj    * I_x_sqr;
		  rhs(2,2) +=         I_x_sqr;

		  // Right Hand Side UR
		  rhs(0,3) += ii*ii * I_x_I_y;
		  rhs(0,4) += ii*jj * I_x_I_y;
		  rhs(0,5) += ii    * I_x_I_y;
		  rhs(1,4) += jj*jj * I_x_I_y;
		  rhs(1,5) += jj    * I_x_I_y;
		  rhs(2,5) +=         I_x_I_y;

		  // Right Hand Side LR
		  rhs(3,3) += ii*ii * I_y_sqr;
		  rhs(3,4) += ii*jj * I_y_sqr;
		  rhs(3,5) += ii    * I_y_sqr;
		  rhs(4,4) += jj*jj * I_y_sqr;
		  rhs(4,5) += jj    * I_y_sqr;
		  rhs(5,5) +=         I_y_sqr;

		}
	      }           
            }// end of if statement: inside image
          }// end of if statement: valid reflectance  
        }// end of for loop over i
      }//end of loop over k

      // Fill in symmetric entries
      rhs(1,0) = rhs(0,1);
      rhs(2,0) = rhs(0,2);
      rhs(2,1) = rhs(1,2);
      rhs(1,3) = rhs(0,4);
      rhs(2,3) = rhs(0,5);
      rhs(2,4) = rhs(1,5);
      rhs(3,0) = rhs(0,3);
      rhs(3,1) = rhs(1,3);
      rhs(3,2) = rhs(2,3);
      rhs(4,0) = rhs(0,4);
      rhs(4,1) = rhs(1,4);
      rhs(4,2) = rhs(2,4);
      rhs(4,3) = rhs(3,4);
      rhs(5,0) = rhs(0,5);
      rhs(5,1) = rhs(1,5);
      rhs(5,2) = rhs(2,5);
      rhs(5,3) = rhs(3,5);
      rhs(5,4) = rhs(4,5);

   
      printRHS(rhs, index, iter);
     
      try {
        solve_symmetric_nocopy(rhs,lhs);
      } catch (ArgumentErr &/*e*/) {
        //             std::cout << "Error @ " << x << " " << y << "\n";
        //             std::cout << "Exception caught: " << e.what() << "\n";
        //             std::cout << "PRERHS: " << pre_rhs << "\n";
        //             std::cout << "PRELHS: " << pre_lhs << "\n\n";
        //             std::cout << "RHS: " << rhs << "\n";
        //             std::cout << "LHS: " << lhs << "\n\n";
        //             std::cout << "DEBUG: " << rhs(0,1) << "   " << rhs(1,0) << "\n\n";
        //             exit(0);
      }


      printLHS_Error(lhs, errorArray[index], index, iter);

      finalTransfArray[index] += lhs;
  
      iter ++;
 
    }
   
  }//index loop ends here

} 


//this is the MP-multi processor version of the above function
void UpdateMatchingParamsLIMA_MP(vector<vector<LOLAShot> > &trackPts, string DRGFilename,  
				 ModelParams modelParams, CoregistrationParams coregistrationParams,  
				 vector<Vector<float, 6> >initTransfArray, vector<Vector<float, 6> >&finalTransfArray, 
				 vector<float> &errorArray )
{

  int numMaxIter = coregistrationParams.maxNumIter;

  DiskImageView<PixelGray<uint8> >   DRG(DRGFilename);
  GeoReference DRGGeo;
  read_georeference(DRGGeo, DRGFilename);

  cout <<"Interpolating the image ...";
  ImageViewRef<PixelGray<uint8> >   interpDRG = interpolate(edge_extend(DRG.impl(),
        ConstantEdgeExtension()),
      BilinearInterpolation());
  cout<<"done."<<endl;


  cout << "Computing the scale factor...";
  float scaleFactor;
  scaleFactor = ComputeScaleFactor(trackPts);
  cout<<"done."<<endl;

  cout << "Computing/Reading the derivative image..."; 
  //to be used in the RELEASE!- DO NOT remove it!
  //DiskCacheImageView<float> x_deriv = derivative_filter(DRG,1,0);
  //DiskCacheImageView<float> y_deriv = derivative_filter(DRG,0,1);

  std::string temp = sufix_from_filename(DRGFilename);

  std::string xDerivFilename = "../results" + prefix_less3_from_filename(temp) + "_x_deriv.tif";
  std::string yDerivFilename = "../results" + prefix_less3_from_filename(temp) + "_y_deriv.tif";

  if ( !boost::filesystem::exists( xDerivFilename ) ) {
    cout << "Computing the x_derivative ..." << endl;
    ImageViewRef<float> temp = derivative_filter(pixel_cast<float>(DRG),1,0);
    DiskImageResourceGDAL rsrc( xDerivFilename,
        temp.format(), Vector2i(512,512) );
    cout << "Writing the x_derivative" << endl;
    block_write_image(rsrc, temp,
        TerminalProgressCallback("asp", "Derivative:") );
    cin.get();
    cout<<"done."<<endl;
  }
  cout << "Reading in the x_deriv from "<<xDerivFilename<<" ..." << endl;

  DiskImageView<float> x_deriv( xDerivFilename );

  if ( !boost::filesystem::exists( yDerivFilename ) ) {
     cout << "Computing the y_derivative ..." << endl;

    ImageViewRef<float> temp = derivative_filter(pixel_cast<float>(DRG),0,1);
    DiskImageResourceGDAL rsrc( yDerivFilename,
        temp.format(), Vector2i(512,512) );
    cout << "Writing the y_derivative" << endl;
    block_write_image(rsrc, temp,
        TerminalProgressCallback("asp", "Derivative:") );
  }
 
  cout << "Reading in the y_deriv from "<<yDerivFilename<<" ... "<< endl;
  DiskImageView<float> y_deriv( yDerivFilename );
  cout<<"done."<<endl;
  

  int row_max, col_max;

  row_max = x_deriv.rows();
  col_max = x_deriv.cols();
   
  cout << "image size: rows: " << row_max << ", cols:"<< col_max << endl;
  int i_C = row_max/2;
  int j_C = col_max/2;


  //copy initTransfArray into finalTransfArray.
  for (int index = 0; index < initTransfArray.size(); index++){
    for (int i = 0; i < 6; i++){
      finalTransfArray[index][i]=initTransfArray[index][i];
    }
  }

   vector<int> ti;
   vector<int> si;
   vector<int> li;
   vector<int> iter;
   vector<int> ii;
   vector<int> jj;
   vector<int> iA;
   vector<int> jA;
   vector<float> I_e_val;
   vector<float> I_x_val;
   vector<float> I_y_val;
   vector<float> I_x_sqr;
   vector<float> I_x_I_y;
   vector<float> I_y_sqr;
   vector<Matrix<float, 6, 6> > rhs;
   vector<Vector<float,6> > lhs; 

   ti.resize(initTransfArray.size());
   si.resize(initTransfArray.size());
   li.resize(initTransfArray.size());
   iter.resize(initTransfArray.size());
   ii.resize(initTransfArray.size());
   jj.resize(initTransfArray.size());
   iA.resize(initTransfArray.size());
   jA.resize(initTransfArray.size());
   I_e_val.resize(initTransfArray.size());
   I_x_val.resize(initTransfArray.size());
   I_y_val.resize(initTransfArray.size());
   I_x_sqr.resize(initTransfArray.size());
   I_x_I_y.resize(initTransfArray.size());
   I_y_sqr.resize(initTransfArray.size());
   rhs.resize(initTransfArray.size());
   lhs.resize(initTransfArray.size());


   int nthreads, tid, chunk; 
   chunk = CHUNKSIZE;
   for (int index = 0; index< initTransfArray.size(); index++){
        iA[index] = 0;
        jA[index] = 0;
        iter[index] = 0;
   }

#pragma omp parallel shared(ti,si, li, iter, ii, jj, iA, jA, I_e_val, I_x_val, I_y_val, I_x_sqr, I_x_I_y, I_y_sqr, rhs, lhs, nthreads,chunk) private(index,tid)
{
  
  tid = omp_get_thread_num();
  if (tid == 0) {
    nthreads = omp_get_num_threads();
    printf("Number of threads = %d\n", nthreads);
  }
  printf("Thread %d starting...\n",tid);


#pragma omp for schedule(dynamic,chunk)

  //for (int index = 0; index < initTransfArray.size(); index++){
  //for (int index = 112; index < 117; index++){

  for (int index = 125; index < 128; index++){
 
    cout << "index = "<< index << endl;

    while( iter[index] <= numMaxIter){ //gradient descent => optimal transform

      //reset rhs & lhs
      for(int i_RHS = 0; i_RHS < 6; i_RHS++){
        for(int j_RHS = 0; j_RHS < 6; j_RHS++){
          rhs[index](i_RHS,j_RHS) = 0.0;
        }
        lhs[index](i_RHS) = 0.0;
      }
 
      //reset the error;
      errorArray[index] = 0.0;

      for (ti[index] = 0; ti[index] < trackPts.size(); ti[index]++){

        for (si[index] = 0; si[index] < trackPts[ti[index]].size(); si[index]++){

          if ((trackPts[ti[index]][si[index]].valid ==1) && (trackPts[ti[index]][si[index]].reflectance !=0)){
           
            //weight = trackPts[k][i].weight_prd;
        
            for (li[index] = 0; li[index] < trackPts[ti[index]][si[index]].LOLAPt.size(); li[index]++){//for each point of a LOLA shot

              if (trackPts[ti[index]][si[index]].LOLAPt[li[index]].s == 3){//center point of a valid shot
    
		iA[index] = (int) floor(finalTransfArray[index][0]*trackPts[ti[index]][si[index]].imgPt[li[index]].x 
                          + finalTransfArray[index][1]*trackPts[ti[index]][si[index]].imgPt[li[index]].y + finalTransfArray[index][2]);
		jA[index] = (int) floor(finalTransfArray[index][3]*trackPts[ti[index]][si[index]].imgPt[li[index]].x 
                          + finalTransfArray[index][4]*trackPts[ti[index]][si[index]].imgPt[li[index]].y + finalTransfArray[index][5]);
                
                // check (iA,jA) are inside the image!
		if ( ( iA[index] >= 0) && ( iA[index] < row_max) && ( jA[index] >= 0) && ( jA[index] < col_max)){
        	  
                  // calculate ii & jj relative to the image center
		  ii[index] = iA[index] - i_C;
		  jj[index] = jA[index] - j_C;

		  I_e_val[index] = interpDRG(jA[index],iA[index]) - scaleFactor*trackPts[ti[index]][si[index]].reflectance;
		  errorArray[index] += abs(I_e_val[index]);

		  //calculate numerical dirivatives (ii,jj).
		  I_x_val[index] = x_deriv(jA[index],iA[index]); 
		  I_y_val[index] = y_deriv(jA[index],iA[index]); 

		  I_x_I_y[index] = I_x_val[index]*I_y_val[index];        
		  I_x_sqr[index] = I_x_val[index]*I_x_val[index];
		  I_y_sqr[index] = I_y_val[index]*I_y_val[index];

		  // Left hand side
		  lhs[index](0) += ii[index] * I_x_val[index] * I_e_val[index];
		  lhs[index](1) += jj[index] * I_x_val[index] * I_e_val[index];
		  lhs[index](2) +=                              I_x_val[index] * I_e_val[index];
		  lhs[index](3) += ii[index] * I_y_val[index] * I_e_val[index];
		  lhs[index](4) += jj[index] * I_y_val[index] * I_e_val[index];
		  lhs[index](5) +=                              I_y_val[index] * I_e_val[index];

		  // Right Hand Side UL
		  rhs[index](0,0) += ii[index]*ii[index] * I_x_sqr[index];
		  rhs[index](0,1) += ii[index]*jj[index] * I_x_sqr[index];
		  rhs[index](0,2) += ii[index]           * I_x_sqr[index];
		  rhs[index](1,1) += jj[index]*jj[index] * I_x_sqr[index];
		  rhs[index](1,2) += jj[index]           * I_x_sqr[index];
		  rhs[index](2,2) +=                       I_x_sqr[index];

		  // Right Hand Side UR
		  rhs[index](0,3) += ii[index]*ii[index] * I_x_I_y[index];
		  rhs[index](0,4) += ii[index]*jj[index] * I_x_I_y[index];
		  rhs[index](0,5) += ii[index]           * I_x_I_y[index];
		  rhs[index](1,4) += jj[index]*jj[index] * I_x_I_y[index];
		  rhs[index](1,5) += jj[index]           * I_x_I_y[index];
		  rhs[index](2,5) +=                       I_x_I_y[index];

		  // Right Hand Side LR
		  rhs[index](3,3) += ii[index]*ii[index] * I_y_sqr[index];
		  rhs[index](3,4) += ii[index]*jj[index] * I_y_sqr[index];
		  rhs[index](3,5) += ii[index]           * I_y_sqr[index];
		  rhs[index](4,4) += jj[index]*jj[index] * I_y_sqr[index];
		  rhs[index](4,5) += jj[index]           * I_y_sqr[index];
		  rhs[index](5,5) +=                       I_y_sqr[index];

		}
	      }           
            }// end of if statement: inside image
          }// end of if statement: valid reflectance  
        }// end of for loop over i
      }//end of loop over k

      // Fill in symmetric entries
      rhs[index](1,0) = rhs[index](0,1);
      rhs[index](2,0) = rhs[index](0,2);
      rhs[index](2,1) = rhs[index](1,2);
      rhs[index](1,3) = rhs[index](0,4);
      rhs[index](2,3) = rhs[index](0,5);
      rhs[index](2,4) = rhs[index](1,5);
      rhs[index](3,0) = rhs[index](0,3);
      rhs[index](3,1) = rhs[index](1,3);
      rhs[index](3,2) = rhs[index](2,3);
      rhs[index](4,0) = rhs[index](0,4);
      rhs[index](4,1) = rhs[index](1,4);
      rhs[index](4,2) = rhs[index](2,4);
      rhs[index](4,3) = rhs[index](3,4);
      rhs[index](5,0) = rhs[index](0,5);
      rhs[index](5,1) = rhs[index](1,5);
      rhs[index](5,2) = rhs[index](2,5);
      rhs[index](5,3) = rhs[index](3,5);
      rhs[index](5,4) = rhs[index](4,5);

   
      printRHS(rhs[index], index, iter[index]);
     
      try {
        solve_symmetric_nocopy(rhs[index],lhs[index]);
      } catch (ArgumentErr &/*e*/) {
        //             std::cout << "Error @ " << x << " " << y << "\n";
        //             std::cout << "Exception caught: " << e.what() << "\n";
        //             std::cout << "PRERHS: " << pre_rhs << "\n";
        //             std::cout << "PRELHS: " << pre_lhs << "\n\n";
        //             std::cout << "RHS: " << rhs << "\n";
        //             std::cout << "LHS: " << lhs << "\n\n";
        //             std::cout << "DEBUG: " << rhs(0,1) << "   " << rhs(1,0) << "\n\n";
        //             exit(0);
      }


      printLHS_Error(lhs[index], errorArray[index], index, iter[index]);

      finalTransfArray[index] += lhs[index];
  
      iter[index] ++;
 
    }
   
  }//index loop ends here

 }//openMP


} 



//this is the MP-multi processor version of the above function
void UpdateMatchingParamsLIDEM_MP(vector<vector<LOLAShot> > &trackPts, string DEMFilename,  
				  ModelParams modelParams,  CoregistrationParams coregistrationParams, 
				  vector<Vector<float, 6> >initTransfArray, vector<Vector<float, 6> >&finalTransfArray, 
				  vector<float> &errorArray )
{

  int numMaxIter = coregistrationParams.maxNumIter;

  DiskImageView<PixelGray<float> >   DEM(DEMFilename);
  GeoReference DEMGeo;
  read_georeference(DEMGeo, DEMFilename);

  cout <<"Interpolating the image ...";
  ImageViewRef<PixelGray<float> >   interpDEM = interpolate(edge_extend(DEM.impl(),
        ConstantEdgeExtension()),
      BilinearInterpolation());
  cout<<"done."<<endl;


  cout << "Computing/Reading the derivative image..."; 
  //to be used in the RELEASE!- DO NOT remove it!
  //DiskCacheImageView<float> x_deriv = derivative_filter(DRG,1,0);
  //DiskCacheImageView<float> y_deriv = derivative_filter(DRG,0,1);

  std::string temp = sufix_from_filename(DEMFilename);

  std::string xDerivFilename = "../results" + prefix_less3_from_filename(temp) + "_x_deriv.tif";
  std::string yDerivFilename = "../results" + prefix_less3_from_filename(temp) + "_y_deriv.tif";

  if ( !boost::filesystem::exists( xDerivFilename ) ) {
    cout << "Computing the x_derivative ..." << endl;
    ImageViewRef<float> temp = derivative_filter(pixel_cast<float>(DEM),1,0);
    DiskImageResourceGDAL rsrc( xDerivFilename,
        temp.format(), Vector2i(512,512) );
    cout << "Writing the x_derivative" << endl;
    block_write_image(rsrc, temp,
        TerminalProgressCallback("asp", "Derivative:") );
    cin.get();
    cout<<"done."<<endl;
  }
  cout << "Reading in the x_deriv from "<<xDerivFilename<<" ..." << endl;

  DiskImageView<float> x_deriv( xDerivFilename );

  if ( !boost::filesystem::exists( yDerivFilename ) ) {
     cout << "Computing the y_derivative ..." << endl;

    ImageViewRef<float> temp = derivative_filter(pixel_cast<float>(DEM),0,1);
    DiskImageResourceGDAL rsrc( yDerivFilename,
        temp.format(), Vector2i(512,512) );
    cout << "Writing the y_derivative" << endl;
    block_write_image(rsrc, temp,
        TerminalProgressCallback("asp", "Derivative:") );
  }
 
  cout << "Reading in the y_deriv from "<<yDerivFilename<<" ... "<< endl;
  DiskImageView<float> y_deriv( yDerivFilename );
  cout<<"done."<<endl;
  

  int row_max, col_max;

  row_max = x_deriv.rows();
  col_max = x_deriv.cols();
   
  cout << "image size: rows: " << row_max << ", cols:"<< col_max << endl;
  int i_C = row_max/2;
  int j_C = col_max/2;


  //copy initTransfArray into finalTransfArray.
  for (int index = 0; index < initTransfArray.size(); index++){
    for (int i = 0; i < 6; i++){
      finalTransfArray[index][i]=initTransfArray[index][i];
    }
  }

   vector<int> ti;
   vector<int> si;
   vector<int> li;
   vector<int> iter;
   vector<int> ii;
   vector<int> jj;
   vector<int> iA;
   vector<int> jA;
   vector<float> I_e_val;
   vector<float> I_x_val;
   vector<float> I_y_val;
   vector<float> I_x_sqr;
   vector<float> I_x_I_y;
   vector<float> I_y_sqr;
   vector<Matrix<float, 6, 6> > rhs;
   vector<Vector<float,6> > lhs; 

   ti.resize(initTransfArray.size());
   si.resize(initTransfArray.size());
   li.resize(initTransfArray.size());
   iter.resize(initTransfArray.size());
   ii.resize(initTransfArray.size());
   jj.resize(initTransfArray.size());
   iA.resize(initTransfArray.size());
   jA.resize(initTransfArray.size());
   I_e_val.resize(initTransfArray.size());
   I_x_val.resize(initTransfArray.size());
   I_y_val.resize(initTransfArray.size());
   I_x_sqr.resize(initTransfArray.size());
   I_x_I_y.resize(initTransfArray.size());
   I_y_sqr.resize(initTransfArray.size());
   rhs.resize(initTransfArray.size());
   lhs.resize(initTransfArray.size());


   int nthreads, tid, chunk; 
   chunk = CHUNKSIZE;
   for (int index = 0; index< initTransfArray.size(); index++){
        iA[index] = 0;
        jA[index] = 0;
        iter[index] = 0;
   }

#pragma omp parallel shared(ti,si, li, iter, ii, jj, iA, jA, I_e_val, I_x_val, I_y_val, I_x_sqr, I_x_I_y, I_y_sqr, rhs, lhs, nthreads,chunk) private(index,tid)
{
  
  tid = omp_get_thread_num();
  if (tid == 0) {
    nthreads = omp_get_num_threads();
    printf("Number of threads = %d\n", nthreads);
  }
  printf("Thread %d starting...\n",tid);


#pragma omp for schedule(dynamic,chunk)

  //for (int index = 0; index < initTransfArray.size(); index++){
  //for (int index = 112; index < 117; index++){

  for (int index = 125; index < 128; index++){
 
    cout << "index = "<< index << endl;

    while( iter[index] <= numMaxIter){ //gradient descent => optimal transform

      //reset rhs & lhs
      for(int i_RHS = 0; i_RHS < 6; i_RHS++){
        for(int j_RHS = 0; j_RHS < 6; j_RHS++){
          rhs[index](i_RHS,j_RHS) = 0.0;
        }
        lhs[index](i_RHS) = 0.0;
      }
 
      //reset the error;
      errorArray[index] = 0.0;

      for (ti[index] = 0; ti[index] < trackPts.size(); ti[index]++){

        for (si[index] = 0; si[index] < trackPts[ti[index]].size(); si[index]++){

          if ((trackPts[ti[index]][si[index]].valid ==1) && (trackPts[ti[index]][si[index]].reflectance !=0)){
           
            //weight = trackPts[k][i].weight_prd;
        
            for (li[index] = 0; li[index] < trackPts[ti[index]][si[index]].LOLAPt.size(); li[index]++){//for each point of a LOLA shot

              if (trackPts[ti[index]][si[index]].LOLAPt[li[index]].s == 3){//center point of a valid shot
    
		iA[index] = (int) floor(finalTransfArray[index][0]*trackPts[ti[index]][si[index]].imgPt[li[index]].x 
                          + finalTransfArray[index][1]*trackPts[ti[index]][si[index]].imgPt[li[index]].y + finalTransfArray[index][2]);
		jA[index] = (int) floor(finalTransfArray[index][3]*trackPts[ti[index]][si[index]].imgPt[li[index]].x 
                          + finalTransfArray[index][4]*trackPts[ti[index]][si[index]].imgPt[li[index]].y + finalTransfArray[index][5]);
                
                // check (iA,jA) are inside the image!
		if ( ( iA[index] >= 0) && ( iA[index] < row_max) && ( jA[index] >= 0) && ( jA[index] < col_max)){
        	  
                  // calculate ii & jj relative to the image center
		  ii[index] = iA[index] - i_C;
		  jj[index] = jA[index] - j_C;
                 
		  I_e_val[index] = interpDEM(jA[index],iA[index]) - trackPts[ti[index]][si[index]].LOLAPt[li[index]].coords[2];
               
		  errorArray[index] += abs(I_e_val[index]);

		  //calculate numerical dirivatives (ii,jj).
		  I_x_val[index] = x_deriv(jA[index],iA[index]); 
		  I_y_val[index] = y_deriv(jA[index],iA[index]); 

		  I_x_I_y[index] = I_x_val[index]*I_y_val[index];        
		  I_x_sqr[index] = I_x_val[index]*I_x_val[index];
		  I_y_sqr[index] = I_y_val[index]*I_y_val[index];

		  // Left hand side
		  lhs[index](0) += ii[index] * I_x_val[index] * I_e_val[index];
		  lhs[index](1) += jj[index] * I_x_val[index] * I_e_val[index];
		  lhs[index](2) +=                              I_x_val[index] * I_e_val[index];
		  lhs[index](3) += ii[index] * I_y_val[index] * I_e_val[index];
		  lhs[index](4) += jj[index] * I_y_val[index] * I_e_val[index];
		  lhs[index](5) +=                              I_y_val[index] * I_e_val[index];

		  // Right Hand Side UL
		  rhs[index](0,0) += ii[index]*ii[index] * I_x_sqr[index];
		  rhs[index](0,1) += ii[index]*jj[index] * I_x_sqr[index];
		  rhs[index](0,2) += ii[index]           * I_x_sqr[index];
		  rhs[index](1,1) += jj[index]*jj[index] * I_x_sqr[index];
		  rhs[index](1,2) += jj[index]           * I_x_sqr[index];
		  rhs[index](2,2) +=                       I_x_sqr[index];

		  // Right Hand Side UR
		  rhs[index](0,3) += ii[index]*ii[index] * I_x_I_y[index];
		  rhs[index](0,4) += ii[index]*jj[index] * I_x_I_y[index];
		  rhs[index](0,5) += ii[index]           * I_x_I_y[index];
		  rhs[index](1,4) += jj[index]*jj[index] * I_x_I_y[index];
		  rhs[index](1,5) += jj[index]           * I_x_I_y[index];
		  rhs[index](2,5) +=                       I_x_I_y[index];

		  // Right Hand Side LR
		  rhs[index](3,3) += ii[index]*ii[index] * I_y_sqr[index];
		  rhs[index](3,4) += ii[index]*jj[index] * I_y_sqr[index];
		  rhs[index](3,5) += ii[index]           * I_y_sqr[index];
		  rhs[index](4,4) += jj[index]*jj[index] * I_y_sqr[index];
		  rhs[index](4,5) += jj[index]           * I_y_sqr[index];
		  rhs[index](5,5) +=                       I_y_sqr[index];

		}
	      }           
            }// end of if statement: inside image
          }// end of if statement: valid reflectance  
        }// end of for loop over i
      }//end of loop over k

      // Fill in symmetric entries
      rhs[index](1,0) = rhs[index](0,1);
      rhs[index](2,0) = rhs[index](0,2);
      rhs[index](2,1) = rhs[index](1,2);
      rhs[index](1,3) = rhs[index](0,4);
      rhs[index](2,3) = rhs[index](0,5);
      rhs[index](2,4) = rhs[index](1,5);
      rhs[index](3,0) = rhs[index](0,3);
      rhs[index](3,1) = rhs[index](1,3);
      rhs[index](3,2) = rhs[index](2,3);
      rhs[index](4,0) = rhs[index](0,4);
      rhs[index](4,1) = rhs[index](1,4);
      rhs[index](4,2) = rhs[index](2,4);
      rhs[index](4,3) = rhs[index](3,4);
      rhs[index](5,0) = rhs[index](0,5);
      rhs[index](5,1) = rhs[index](1,5);
      rhs[index](5,2) = rhs[index](2,5);
      rhs[index](5,3) = rhs[index](3,5);
      rhs[index](5,4) = rhs[index](4,5);

   
      printRHS(rhs[index], index, iter[index]);
     
      try {
        solve_symmetric_nocopy(rhs[index],lhs[index]);
      } catch (ArgumentErr &/*e*/) {
        //             std::cout << "Error @ " << x << " " << y << "\n";
        //             std::cout << "Exception caught: " << e.what() << "\n";
        //             std::cout << "PRERHS: " << pre_rhs << "\n";
        //             std::cout << "PRELHS: " << pre_lhs << "\n\n";
        //             std::cout << "RHS: " << rhs << "\n";
        //             std::cout << "LHS: " << lhs << "\n\n";
        //             std::cout << "DEBUG: " << rhs(0,1) << "   " << rhs(1,0) << "\n\n";
        //             exit(0);
      }


      printLHS_Error(lhs[index], errorArray[index], index, iter[index]);

      finalTransfArray[index] += lhs[index];
  
      iter[index] ++;
 
    }
   
  }//index loop ends here

 }//openMP


} 
