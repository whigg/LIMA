// __BEGIN_LICENSE__
// Copyright (C) 2006, 2007 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#include <iomanip>
#include <math.h>
#include <boost/tokenizer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <vw/Core.h>
#include <vw/Math/Matrix.h>
#include <vw/Cartography.h>
#include <vw/Cartography/SimplePointImageManipulation.h>
#include <vw/FileIO.h>
#include <asp/IsisIO.h>
#include <asp/IsisIO/IsisCameraModel.h>

#include "coregister.h"
#include "util.h"
#include "tracks.h"

using namespace vw;
using namespace vw::cartography;
using namespace vw::math;

//needed for map to cam back projection - START
#include <Camera.h>
#include <Pvl.h>
#include <CameraFactory.h>
#include <CameraDetectorMap.h>
//needed for map to cam back projection - END

//Constructor for pointCloud which is really just a Vector3 
// with some extra data fields.
pointCloud::pointCloud
  (
  Vector3 coords,
  int     y,
  int     mo,
  int     d,
  int     h,
  int     mi,
  float   se,
  int     detector 
  )
  :
  Vector<double,3>( coords )
  {
  year = y;
  month = mo;
  day = d;
  hour = h;
  min = mi;
  sec = se;
  s = detector;
  }

void
LOLAShot::init
  (
  vector<pointCloud> pc,
  vector<imgPoint>   ip,
  vector<DEMPoint>   dp,
  int                v,
  int                cpi,
  float              r,
  float              si,
  int                ca,
  float              fr,
  float              fpr,
  float              wr,
  float              fpL,
  float              fL
  )
  {
  LOLAPt = pc;
  imgPt = ip;
  DEMPt = dp;
  valid = v;
  centerPtIndex = cpi;
  reflectance = r;
  synthImage = si;
  calc_acp = ca;
  filter_response = fr;
  featurePtRefl = fpr;
  weightRefl = wr;
  featurePtLOLA = fpL;
  filresLOLA = fL;
  }

LOLAShot::LOLAShot( vector<pointCloud> pcv )
  {
  LOLAShot::init( pcv );
  }
LOLAShot::LOLAShot( pointCloud pt )
  {
  vector<pointCloud> pcv( 1, pt );
  LOLAShot::init( pcv );
  }

bool isTimeDiff( pointCloud p, pointCloud c, float timeThresh){
  using namespace boost::posix_time;
  using namespace boost::gregorian;
  float intpart;
  ptime first( date(p.year,p.month,p.day),
               hours(p.hour) +minutes(p.min)+seconds(p.sec)
               +microseconds( modf(p.sec, &intpart) * 1000000 ) );
  ptime second( date(c.year,c.month,c.day),
                hours(c.hour)+minutes(c.min)+seconds(c.sec)
                +microseconds( modf(c.sec, &intpart) * 1000000 ) );

  if( first == second ) { return false; }

  time_duration duration( second - first );
  if( duration.is_negative() ) { duration = duration.invert_sign(); }

  if( duration.total_seconds() < timeThresh ){ return false; }
  return true;
}


vector<vector<LOLAShot> > LOLAFileRead( const string& f ) {
  // This is specifically for reading the RDR_*PointPerRow_csv_table.csv files only.
  // It will ignore lines which do not start with year (or a value that can be converted
  // into an integer greater than zero, specfically).
  ifstream file( f.c_str() );
  if( !file ) {
    vw_throw( vw::IOErr() << "Unable to open track file \"" << f << "\"" );
  }

  vector<vector<LOLAShot> > trackPts(1);

  // Prepare the tokenizer stuff.
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep(" ,");

  // file >> ignoreLine; // Skip header line
 
  // Establish 3d bounding box for acceptable LOLA data. 
  //Vector3 LOLAmin( -180, -90, 1720 );
  //Vector3 LOLAmax(  360,  90, 1750 );
  BBox3 LOLArange( Vector3(-180, -90, 1720), Vector3(360,  90, 1750) );

  int trackIndex = 0;
  pointCloud currPt;
  pointCloud prevPt;
  string line;
  while( getline(file, line, '\n') ) {
    /*
    // For Debugging: this reads the first line, parses, numbers, and prints
    tokenizer tokens( line, sep );
    int counter(0);
    for( tokenizer::iterator token = tokens.begin();
    		token != tokens.end();
    		++token, counter++)
    	{
    	cerr	<< boost::lexical_cast<std::string>(counter) 
    			<< ". " << *token << endl;
    	}
    exit(1);
    */

    tokenizer tokens( line, sep );
    tokenizer::iterator token = tokens.begin();

    istringstream date_s( *token );
    date_s >>  setw(4) >> currPt.year
	       >> ignoreOne // -
	       >> setw(2) >> currPt.month
	       >> ignoreOne // -
	       >> setw(2) >> currPt.day
	       >> ignoreOne // T
	       >> setw(2) >> currPt.hour
	       >> ignoreOne // :
	       >> setw(2) >> currPt.min
	       >> ignoreOne // :
	       >> currPt.sec;
    //cout<<"year "<< currPt.year << endl;
    if( currPt.year <= 0 ) { continue; }
    //cout << currPt.year <<' '<< currPt.month <<' '<< currPt.day 
    //	<<' '<< currPt.hour <<' '<< currPt.min <<' '<< currPt.sec << endl;

    ++token;
    istringstream lon_s( *token );
    lon_s >> currPt.x(); // Pt_Longitude
    // cout<<"lon "<< currPt.coords	s_s >> "s_s"<<currPt.s; // S(0) << endl;

    ++token;
    istringstream lat_s( *token );
    lat_s >> currPt.y(); // Pt_Latitude

    ++token;
    istringstream rad_s( *token );
    rad_s >> currPt.z(); // Pt_Radius
    // cout<<"radius "<< currPt.coords(2) << endl;

    advance( token, 8 );
    istringstream s_s( *token );
    s_s >>currPt.s; // S
    //cout<<"s_s "<<currPt.s<<endl;
    
    if( LOLArange.contains(currPt) ){
      if( trackPts[trackIndex].empty() ) {
        trackPts[trackIndex].push_back( LOLAShot(currPt) );
      }
      else {
        if( isTimeDiff(prevPt, currPt, 3000) ) { //new track
          trackIndex++;
          trackPts.resize(trackIndex+1); //start new track
          trackPts[trackIndex].push_back( LOLAShot(currPt) ); //put the new shot in it
        }
        else { //same track
          if( isTimeDiff(prevPt, currPt, 0) ) { //new shot
            trackPts[trackIndex].push_back( LOLAShot(currPt) );
          }
          else { //same shot
            trackPts[trackIndex].back().LOLAPt.push_back( currPt );
          }
        }
      }

      //copy current pc into prevPt
      prevPt = currPt;
    }
  } 
  file.close();
  return trackPts; 
}

Vector2 ComputeMinMaxValuesFromCub( const string& cubFilename)
{
  Vector2 minmax;
  boost::shared_ptr<DiskImageResource> rsrc( new DiskImageResourceIsis(cubFilename) );
  double nodataVal = rsrc->nodata_read();
  DiskImageView<PixelGray<float> > isis_view( rsrc );
  PixelGray<float> minVal = std::numeric_limits<float>::max();
  PixelGray<float> maxVal = std::numeric_limits<float>::min();
  min_max_pixel_values( create_mask(isis_view,nodataVal), minVal, maxVal );
  minmax(0) = minVal;
  minmax(1) = maxVal;
  //cout<<"min="<<minVal<<", max="<<maxVal<<endl;
  return minmax;
}

/*
Vector2 ComputeMinMaxValuesFromDEM(string demFilename)
{
  Vector2 minmax;
  boost::shared_ptr<DiskImageResource> rsrc( new DiskImageResourceIsis(demFilename) );
  double nodataVal = rsrc->nodata_read();
  //cout<<"nodaval:"<<nodataVal<<endl;
  DiskImageView<PixelGray<uint16> > dem( rsrc );
  int width = dem.cols();
  int height = dem.rows();
  float minVal = 100000000.0;
  float maxVal = -100000000.0;
  for (int i = 0; i < height; i++){
    for (int j = 0; j < width; j++){
      
      if ((dem(j,i) < minVal) && (dem(j,i) > nodataVal)){
	minVal = dem(j,i);
      }
      if ((dem(j,i) > maxVal) && (dem(j,i) > nodataVal)){
	maxVal = dem(j,i);
      }
    }
  }
  minmax(0) = minVal;
  minmax(1) = maxVal;
  cout<<"min="<<minVal<<", max="<<maxVal<<endl;

  return minmax;
}
*/
int GetAllPtsFromCub(vector<vector<LOLAShot > > &trackPts, string cubFilename)
{

  vector<pointCloud> LOLAPts;
  
  boost::shared_ptr<DiskImageResource> rsrc( new DiskImageResourceIsis(cubFilename) );
  double nodata_value = rsrc->nodata_read();
  cout<<"no_data_val="<<nodata_value<<endl; 

  DiskImageView<PixelGray<float> > isis_view( rsrc );
  int width = isis_view.cols();
  int height = isis_view.rows();
  camera::IsisCameraModel model(cubFilename);

  //calculate the min max value of the image
  Vector2 minmax = ComputeMinMaxValuesFromCub(cubFilename);

  float minTrackVal =  1000000.0;
  float maxTrackVal = -1000000.0;

  int numValidImgPts = 0;

  InterpolationView<EdgeExtensionView<DiskImageView<PixelGray<float> >, ConstantEdgeExtension>, BilinearInterpolation> interpImg
    = interpolate(isis_view, BilinearInterpolation(), ConstantEdgeExtension());
  
  for(unsigned int k = 0; k < trackPts.size();k++){
    for(unsigned int i = 0; i < trackPts[k].size(); i++){
      
      trackPts[k][i].valid = 1; 

      LOLAPts = trackPts[k][i].LOLAPt;
  
      trackPts[k][i].imgPt.resize(LOLAPts.size());

      for (unsigned int j = 0; j < LOLAPts.size(); j++){
          
	    float lon = LOLAPts[j].x();
	    float lat = LOLAPts[j].y();
	    float rad = LOLAPts[j].z();
            
            Vector3 lon_lat_rad (lon,lat,rad*1000);
            Vector3 xyz = cartography::lon_lat_radius_to_xyz(lon_lat_rad);
            Vector2 cub_pix = model.point_to_pixel(xyz);
            float x = cub_pix[0];
            float y = cub_pix[1];
            //check that (x,y) are within the image boundaries
	    if ((x>=0) && (y>=0) && (x<width) && (y<height)){//valid position  
              //check for valid data as well
              if (interpImg(x, y)>minmax(0)){//valid values
		 trackPts[k][i].imgPt[j].val = interpImg(x, y);
	         trackPts[k][i].imgPt[j].x = cub_pix[0];
	         trackPts[k][i].imgPt[j].y = cub_pix[1];
                 if (interpImg(x,y) < minTrackVal){
		     minTrackVal = interpImg(x,y);
                 }
		 if (interpImg(x,y) > maxTrackVal){
		     maxTrackVal = interpImg(x,y);
                 }
                 numValidImgPts++;
	      }
              else{//invalidate the point
                 trackPts[k][i].valid = 0;
              }
	    }
            else{ //invalidate the point  
                 trackPts[k][i].valid = 0; 
            }
	
      }
      /*
      // this must go to computeReflectance function - START
      //check for valid shot with 3 points to compute reflectance
      pointCloud centerPt  = GetPointFromIndex( LOLAPts, 1);
      pointCloud topPt     = GetPointFromIndex( LOLAPts, 3);
      pointCloud leftPt    = GetPointFromIndex( LOLAPts, 2);
      if ((centerPt.s == -1) || (topPt.s == -1) || (leftPt.s == -1) || (LOLAPts.size() > 5)){//invalid LOLA shot
          trackPts[k][i].valid = 0; 
      }
      // this must go to computeReflectance function - END
      */
    }//i  
  }//k
  cout <<"minTrackVal="<<minTrackVal<<", maxTrackVal="<<maxTrackVal<<endl;
  
  return numValidImgPts;
}


//computes the scale factor for all tracks at once
float ComputeScaleFactor(vector<vector<LOLAShot > >&trackPts)
{
  float nominator = 0.0;
  int numValidPts = 0;
  float scaleFactor;// = 1;

  //Vector2 minmax = ComputeMinMaxValuesFromCub(string cubFilename);

  for(unsigned int k = 0; k < trackPts.size();k++){
    for(unsigned int i = 0; i < trackPts[k].size(); i++){
      if ((trackPts[k][i].valid == 1) && (trackPts[k][i].reflectance != 0) &&(trackPts[k][i].reflectance != -1)){//valid track and non-zero reflectance

        //update the nominator for the center point

        for (unsigned int j = 0; j < trackPts[k][i].LOLAPt.size(); j++){
          if (trackPts[k][i].LOLAPt[j].s == 1){
            //cout<< trackPts[k][i].imgPt[j].val/trackPts[k][i].reflectance<<endl;
            nominator = nominator + (trackPts[k][i].imgPt[j].val/*-minmax(0)*/)/trackPts[k][i].reflectance;
            numValidPts++;
          }
        }

        //update the denominator
        //numValidPts++;
      }
    }
  }

  //cout<<"NUM_VALID_POINTS="<<numValidPts<<endl;
  if (numValidPts != 0){ 
    scaleFactor = nominator/numValidPts;
  }
  else{
    //invalid scaleFactor, all tracks are invalid
    scaleFactor = -1;
  }
  return scaleFactor;
}

void GainBiasAccumulator( const vector<LOLAShot>& trackPts, 
                                float&            sum_rfl, 
                                float&            sum_img, 
                                float&            sum_rfl_2, 
                                float&            sum_rfl_img, 
                                int&              numValidPts ){
  for (unsigned int i = 0; i < trackPts.size(); ++i){
    if ((trackPts[i].valid == 1) && (trackPts[i].reflectance > 0)){
      //update the nominator for the center point
      for( unsigned int j = 0; j < trackPts[i].LOLAPt.size(); ++j ){
	    if( trackPts[i].LOLAPt[j].s == 1 ){
	      sum_rfl     += trackPts[i].reflectance;
	      sum_rfl_2   += trackPts[i].reflectance*trackPts[i].reflectance;
	      sum_rfl_img += trackPts[i].reflectance*(trackPts[i].imgPt[j].val);
	      sum_img     += trackPts[i].imgPt[j].val;
	      ++numValidPts;
	    }
      }
    }
  }
}

Vector2 GainBiasSolver( const float& sum_rfl, 
                        const float& sum_img, 
                        const float& sum_rfl_2, 
                        const float& sum_rfl_img, 
                        const int&   numValidPts ){
  Matrix<float,2,2> rhs;
  Vector<float,2> lhs;

  rhs(0,0) = sum_rfl_2;
  rhs(0,1) = sum_rfl;
  rhs(1,0) = sum_rfl;
  rhs(1,1) = numValidPts;
  lhs(0) = sum_rfl_img;
  lhs(1) = sum_img;
  solve_symmetric_nocopy(rhs,lhs);
  return lhs;
}


//computes the gain and bias factor for each track
Vector2 ComputeGainBiasFactor( const vector<LOLAShot>& trackPts ) {
  int numValidPts = 0;
  float sum_rfl = 0.0; 
  float sum_img = 0.0;
  float sum_rfl_2 = 0.0;
  float sum_rfl_img = 0.0;
  Vector2 gain_bias;

  GainBiasAccumulator( trackPts, sum_rfl, sum_img, sum_rfl_2, sum_rfl_img, numValidPts );

  //cout<<"NUM_VALID_POINTS="<<numValidPts<<endl;
  //if (numValidPts != 0){
  if( numValidPts > 20 ){ 
    gain_bias = GainBiasSolver( sum_rfl, sum_img, sum_rfl_2, sum_rfl_img, numValidPts );
  }
  else{
    //invalid scaleFactor, all tracks are invalid
    gain_bias(0) = 0.0;
    gain_bias(1) = 0.0;
  }
  return gain_bias;
}

//computes the gain and bias factor for all tracks at once
Vector2 ComputeGainBiasFactor( const vector<vector<LOLAShot> >& trackPts ) {
  int numValidPts = 0;
  float sum_rfl = 0.0; 
  float sum_img = 0.0;
  float sum_rfl_2 = 0.0;
  float sum_rfl_img = 0.0;
  Vector2 gain_bias;

  for( unsigned int k = 0; k < trackPts.size(); ++k){
    GainBiasAccumulator( trackPts[k], sum_rfl, sum_img, sum_rfl_2, sum_rfl_img, numValidPts );
  }

  //cout<<"NUM_VALID_POINTS="<<numValidPts<<endl;
  if (numValidPts != 0){ 
    gain_bias = GainBiasSolver( sum_rfl, sum_img, sum_rfl_2, sum_rfl_img, numValidPts );
  }
  else{
    //invalid scaleFactor, all tracks are invalid
    gain_bias(0) = 0.0;
    gain_bias(1) = 0.0;
  }
  return gain_bias;
}

Vector3 ComputePlaneNormalFrom3DPoints(vector<Vector3> pointArray)
{
  Vector3 normal;
  Matrix<float,5,4> rhs;
  Vector<float,3> lhs;
  for (unsigned int i = 0; i < pointArray.size(); i++){
    rhs(i,0)=pointArray[i][0];
    rhs(i,1)=pointArray[i][1]; 
    rhs(i,2)=pointArray[i][2];
    rhs(i,3)=1;
  }
  
  solve_symmetric_nocopy(rhs,lhs);
  return normal;

}

Vector3 ComputeNormalFrom3DPointsGeneral(Vector3 p1, Vector3 p2, Vector3 p3) {
  return -normalize(cross_prod(p2-p1,p3-p1));
}

float ComputeLunarLambertianReflectanceFromNormal(Vector3 sunPos, Vector3 viewPos, Vector3 xyz, Vector3 normal) 
{
  float reflectance;
  float L;

  //compute /mu_0 = cosine of the angle between the light direction and the surface normal.
  //sun coordinates relative to the xyz point on the Moon surface
  //Vector3 sunDirection = -normalize(sunPos-xyz);
  Vector3 sunDirection = normalize(sunPos-xyz);
  float mu_0 = dot_prod(sunDirection,normal);

  //compute  /mu = cosine of the angle between the viewer direction and the surface normal.
  //viewer coordinates relative to the xyz point on the Moon surface
  Vector3 viewDirection = normalize(viewPos-xyz);
  float mu = dot_prod(viewDirection,normal);

  //compute the phase angle /alpha between the viewing direction and the light source direction
  float rad_alpha, deg_alpha;
  float cos_alpha;

  cos_alpha = dot_prod(sunDirection,viewDirection);
  if ((cos_alpha > 1)||(cos_alpha< -1)){
    printf("cos_alpha error\n");
  }

  rad_alpha = acos(cos_alpha);
  deg_alpha = rad_alpha*180.0/M_PI;

  //printf("deg_alpha = %f\n", deg_alpha);

  //Bob Gaskell's model
  //L = exp(-deg_alpha/60.0);

#if 0 // trey
  // perfectly valid for alpha to be greater than 90?
  if (deg_alpha > 90){
    //printf("Error!!: rad_alpha = %f, deg_alpha = %f\n", rad_alpha, deg_alpha);
    return(0.0);
  }
  if (deg_alpha < -90){
    //printf("Error!!: rad_alpha = %f, deg_alpha = %f\n", rad_alpha, deg_alpha);
    return(0.0);
  }
#endif

  //Alfred McEwen's model
  float A = -0.019;
  float B =  0.000242;//0.242*1e-3;
  float C = -0.00000146;//-1.46*1e-6;

  L = 1.0 + A*deg_alpha + B*deg_alpha*deg_alpha + C*deg_alpha*deg_alpha*deg_alpha;

  //        std::cout << " sun direction " << sunDirection << " view direction " << viewDirection << " normal " << normal;
  //        std::cout << " cos_alpha " << cos_alpha << " incident " << mu_0 << " emission " << mu;
  //printf(" deg_alpha = %f, L = %f\n", deg_alpha, L);

  //if (mu_0 < 0.15){ //incidence angle is close to 90 deg
  if (mu_0 < 0.0){
    //mu_0 = 0.15;
    return (0.0);
  }

  if (mu < 0.0){ //emission angle is > 90
    mu = 0.0;
    //return (0.0);
  }

  if (mu_0 + mu == 0){
    //printf("negative reflectance\n");
    reflectance = 0.0;
  }
  else{
    reflectance = 2*L*mu_0/(mu_0+mu) + (1-L)*mu_0;
  }
  if (reflectance < 0){
    //printf("negative reflectance\n");
    reflectance = 0;
  }
  return reflectance;
}

int ComputeAllReflectance(       vector< vector<LOLAShot> >& shots,  
                           const Vector3&                    cameraPosition, 
                           const Vector3&                    lightPosition) {
  int numValidReflPts = 0;

  float minReflectance = std::numeric_limits<float>::max();
  float maxReflectance = std::numeric_limits<float>::min();
 
  for( unsigned int k = 0; k < shots.size(); ++k ){
    for( unsigned int i = 0; i < shots[k].size(); ++i ){
      pointCloud centerPt = GetPointFromIndex( shots[k][i].LOLAPt, 1 );
      pointCloud topPt    = GetPointFromIndex( shots[k][i].LOLAPt, 3 );
      pointCloud leftPt   = GetPointFromIndex( shots[k][i].LOLAPt, 2 );
      
      if( (centerPt.s != -1) && (topPt.s != -1) && (leftPt.s != -1) 
          && (shots[k][i].LOLAPt.size() <= 5) ){
        centerPt.z() *= 1000;
        topPt.z()    *= 1000;
        leftPt.z()   *= 1000;
        
        Vector3 xyz = lon_lat_radius_to_xyz(centerPt);
        Vector3 xyzTop = lon_lat_radius_to_xyz(topPt);
        Vector3 xyzLeft = lon_lat_radius_to_xyz(leftPt);

        Vector3 normal = ComputeNormalFrom3DPointsGeneral(xyz, xyzLeft, xyzTop);

        shots[k][i].reflectance = ComputeLunarLambertianReflectanceFromNormal(lightPosition, cameraPosition, xyz, normal); 
        
        if( shots[k][i].reflectance != 0 ){ 
          ++numValidReflPts;

          if( shots[k][i].reflectance < minReflectance ){
            minReflectance = shots[k][i].reflectance;
          } 
          if( shots[k][i].reflectance > maxReflectance ){
            maxReflectance = shots[k][i].reflectance;
          }
        }        
      }
      else{ shots[k][i].reflectance = -1; }
    }//i
  }//k
 
  //cout<<"NEW:"<<"minReflectance="<<minReflectance<<", maxReflectance="<<maxReflectance<<endl;

  return numValidReflPts;
}

pointCloud GetPointFromIndex( const vector<pointCloud>&  LOLAPts, const int index ) {
  for( unsigned int i = 0; i < LOLAPts.size(); ++i ) {
    if( LOLAPts[i].s == index ){
      return LOLAPts[i];
    }
  }

  // At the moment, this returns an "invalid" point.
  pointCloud pt;
  pt.s = -1; //invalid
  return pt;
  // But it really should throw, like this:
  // vw_throw( ArgumentErr() << "Couldn't find a point with detector number " << index );
  // However, ComputeAllReflectance() and GetAllPtsFromImage() must have a test, first.
}


void SaveReflectancePoints( const vector<vector<LOLAShot> >& trackPts, 
                            const Vector2&                   gain_bias,
                            const string&                    filename) {
  boost::filesystem::path p( filename );
  for( unsigned int k = 0; k < trackPts.size(); ++k ){
    ostringstream os;
    os << p.stem().string() << "_" << k << ".txt";

    ofstream file( os.str().c_str() );
    if( !file ) {
      vw_throw( ArgumentErr() << "Can't open reflectance output file " << os.str() );
    }
    
    for( unsigned int i = 0; i < trackPts[k].size(); ++i){
      if ( (trackPts[k][i].valid == 1) && 
           (trackPts[k][i].reflectance > 0) ){//valid track and non-zero reflectance
        file << ( gain_bias(0)*trackPts[k][i].reflectance + gain_bias(1) ) << endl;
      }
      else{ file << "-1" << endl; }
    }
    file.close();
  }
}

//saves the image points corresponding to a detectNum
void SaveImagePoints( const vector<vector<LOLAShot> >& allTracks,
                      const int&                       detectNum,
                      const string&                    filename) {
  boost::filesystem::path p( filename );
  for (unsigned int k = 0; k < allTracks.size(); ++k ){
    ostringstream os;
    os << p.stem().string() << "_" << k << ".txt";
    
    ofstream file( os.str().c_str() );
    if( !file ) {
      vw_throw( ArgumentErr() << "Can't open image point output file " << os.str() );
    }

    for( unsigned int i = 0; i < allTracks[k].size(); ++i){
      bool found = false;
      for( unsigned int j = 0; j < allTracks[k][i].LOLAPt.size(); ++j){
        if( (allTracks[k][i].LOLAPt[j].s == detectNum) && 
            (allTracks[k][i].valid       == 1)           ){
          found = true;
          file <<  allTracks[k][i].imgPt[j].val << endl;
        }
      }
      if( !found ){ file << "-1" << endl; }
    }
    file.close();
  }
}

//saves the altitude  points (LOLA) corresponding to a sensor ID = detectNum
void SaveAltitudePoints( const vector<vector<LOLAShot> >& tracks,
                         const int&                       detectNum,
                         const string&                    filename) {
  boost::filesystem::path p( filename );
  for (unsigned int t = 0; t < tracks.size(); ++t ){
    ostringstream os;
    os << p.stem().string() << "_" << t << ".txt";
    
    ofstream file( os.str().c_str() );
    if( !file ) {
      vw_throw( ArgumentErr() << "Can't open altitude output file " << os.str() );
    }

    for( unsigned int s = 0; s < tracks[t].size(); ++s ){
      bool found = false;
      for( unsigned int j = 0; j < tracks[t][s].LOLAPt.size(); ++j ){
        if( (tracks[t][s].LOLAPt[j].s == detectNum) && 
            (tracks[t][s].valid       == 1)           ){
          found = true;
          file << tracks[t][s].LOLAPt[j].z() << endl;
        }
      }
      if( !found ){ file << "-1" << endl; }
    }
    file.close();
  }
}


void UpdateGCP(vector<vector<LOLAShot> > trackPts, Vector<float, 6> optimalTransfArray, 
               string cubFile, vector<gcp> &gcpArray, Vector2 centroid)
{

   int index = 0;
    for (unsigned int t=0; t<trackPts.size(); t++){
      //int featureIndex = 0;
      for (unsigned int s=0; s<(unsigned int)trackPts[t].size(); s++){
	if ( (trackPts[t][s].featurePtLOLA == 1)/* && (trackPts[t][s].valid ==1 )*/){
          if (trackPts[t][s].valid == 1){          
	    gcpArray[index].filename.push_back(cubFile);
         
	    float i = (optimalTransfArray[0]*(trackPts[t][s].imgPt[2].x - centroid(0)) + 
                       optimalTransfArray[1]*(trackPts[t][s].imgPt[2].y - centroid(1)) + 
                       optimalTransfArray[2] + centroid(0));
	    float j = (optimalTransfArray[3]*(trackPts[t][s].imgPt[2].x - centroid(0)) + 
		       optimalTransfArray[4]*(trackPts[t][s].imgPt[2].y - centroid(1)) + 
                       optimalTransfArray[5] + centroid(1));
          
            gcpArray[index].x.push_back(i);
	    gcpArray[index].y.push_back(j);
	    gcpArray[index].x_before.push_back(trackPts[t][s].imgPt[2].x);
	    gcpArray[index].y_before.push_back(trackPts[t][s].imgPt[2].y);
            gcpArray[index].trackIndex = t;
            gcpArray[index].shotIndex = s;//featureIndex;
	  }
          //featureIndex++;
          index++;
	}
      }
    }
    
}

void UpdateGCP(vector<vector<LOLAShot> > trackPts, 
               vector<Vector4> matchArray, vector<float> errorArray, 
               string camCubFile, string mapCubFile, 
               vector<gcp> &gcpArray, float downsample_factor)
{

   std::vector<float> map_pixel;
   map_pixel.resize(2);
   
   std::vector<float> map_pixel_init;
   map_pixel_init.resize(2);

   std::vector<float> cam_pixel;
   cam_pixel.resize(2);
   
   std::vector<float> cam_pixel_init;
   cam_pixel_init.resize(2);

   Isis::Pvl label( mapCubFile);
   Isis::Camera* camera = Isis::CameraFactory::Create( label );
  // Note that ISIS is different from C. They start their index at 1.
  
   int featureIndex = 0;
   int validFeatureIndex = 0;

   for (unsigned int t = 0; t < trackPts.size(); t++){
     for (unsigned int s = 0; s < (unsigned int)trackPts[t].size(); s++){
       if (trackPts[t][s].featurePtLOLA == 1){
         if ((trackPts[t][s].valid == 1) && (trackPts[t][s].reflectance != 0) && (trackPts[t][s].reflectance != -1)){
          
	    gcpArray[featureIndex].filename.push_back(camCubFile);

            float i = matchArray[validFeatureIndex](2);
	    float j = matchArray[validFeatureIndex](3);
        
            map_pixel[0] = i;
            map_pixel[1] = j; 

            // convert a map projected pixel location to the
            // original image coordinate system.
             
            camera->SetImage(map_pixel[0], map_pixel[1]);
            cam_pixel[0] = camera->DetectorMap()->ParentSample();
            cam_pixel[1] = camera->DetectorMap()->ParentLine();
	   	
	    gcpArray[featureIndex].x.push_back(cam_pixel[0]/downsample_factor);
	    gcpArray[featureIndex].y.push_back(cam_pixel[1]/downsample_factor);

	    map_pixel_init[0]=trackPts[t][s].imgPt[2].x;
            map_pixel_init[1]=trackPts[t][s].imgPt[2].y;
            
            camera->SetImage(map_pixel_init[0], map_pixel_init[1]);
            cam_pixel_init[0] = camera->DetectorMap()->ParentSample();
            cam_pixel_init[1] = camera->DetectorMap()->ParentLine();
	       
            gcpArray[featureIndex].x_before.push_back(cam_pixel_init[0]/downsample_factor);
	    gcpArray[featureIndex].y_before.push_back(cam_pixel_init[1]/downsample_factor);
            gcpArray[featureIndex].trackIndex = t;
            gcpArray[featureIndex].shotIndex = s;

            validFeatureIndex++;
    
	    }//valid==1
	
	  featureIndex++;
	}
      }
    }

    // delete remaining ISIS objects
    delete camera;
    
}

void UpdateGCP(vector<vector<LOLAShot> > trackPts, Vector<float, 6> optimalTransfArray, 
               string camCubFile, string mapCubFile, vector<gcp> &gcpArray, Vector2 centroid, 
               float downsample_factor)
{

   std::vector<float> map_pixel;
   map_pixel.resize(2);
   
   std::vector<float> map_pixel_init;
   map_pixel_init.resize(2);

   std::vector<float> cam_pixel;
   cam_pixel.resize(2);
   
   std::vector<float> cam_pixel_init;
   cam_pixel_init.resize(2);

   Isis::Pvl label( mapCubFile);
   Isis::Camera* camera = Isis::CameraFactory::Create( label );
  // Note that ISIS is different from C. They start their index at 1.
  
   int index = 0;
   for (unsigned int t=0; t<trackPts.size(); t++){
     for (unsigned int s=0; s<(unsigned int)trackPts[t].size(); s++){
       if ((trackPts[t][s].featurePtLOLA == 1)/* && (trackPts[t][s].valid ==1)*/){
         if (trackPts[t][s].valid == 1){          
	    gcpArray[index].filename.push_back(camCubFile);
         
	    float i = (optimalTransfArray[0]*(trackPts[t][s].imgPt[2].x - centroid(0)) + 
                       optimalTransfArray[1]*(trackPts[t][s].imgPt[2].y - centroid(1)) + 
                       optimalTransfArray[2] + centroid(0));
	    float j = (optimalTransfArray[3]*(trackPts[t][s].imgPt[2].x - centroid(0)) + 
		       optimalTransfArray[4]*(trackPts[t][s].imgPt[2].y - centroid(1)) + 
                       optimalTransfArray[5] + centroid(1));

            map_pixel[0] = i;
            map_pixel[1] = j; 

            // convert a map projected pixel location to the
            // original image coordinate system.
             
            camera->SetImage(map_pixel[0], map_pixel[1]);
            cam_pixel[0] = camera->DetectorMap()->ParentSample();
            cam_pixel[1] = camera->DetectorMap()->ParentLine();
	   	
	    gcpArray[index].x.push_back(cam_pixel[0]/downsample_factor);
	    gcpArray[index].y.push_back(cam_pixel[1]/downsample_factor);

	    map_pixel_init[0]=trackPts[t][s].imgPt[2].x;
            map_pixel_init[1]=trackPts[t][s].imgPt[2].y;
            
            camera->SetImage(map_pixel_init[0], map_pixel_init[1]);
            cam_pixel_init[0] = camera->DetectorMap()->ParentSample();
            cam_pixel_init[1] = camera->DetectorMap()->ParentLine();
	       
            gcpArray[index].x_before.push_back(cam_pixel_init[0]/downsample_factor);
	    gcpArray[index].y_before.push_back(cam_pixel_init[1]/downsample_factor);
            gcpArray[index].trackIndex = t;
            gcpArray[index].shotIndex = s;
    
	  }//valid==1
	
	  index++;
	}
      }
    }

    // delete remaining ISIS objects
    delete camera;
    
}

void SaveGCPoints(vector<gcp> gcpArray,  string gcpFilename)
{

  for (unsigned int i = 0; i < gcpArray.size(); i++){
    
       //check if this GCP is valid
       int numFiles = gcpArray[i].filename.size();
       if (numFiles > 0){
	   stringstream ss;
	   ss<<i;

           stringstream featureIndexString;
           featureIndexString<<gcpArray[i].shotIndex;
           stringstream trackIndexString;
	   trackIndexString<<gcpArray[i].trackIndex;
           //string this_gcpFilename = gcpFilename+"_"+ss.str()+".gcp";
	   
           string this_gcpFilename = gcpFilename+"_"+trackIndexString.str()+"_"+featureIndexString.str()+".gcp";
	   FILE *fp = fopen(this_gcpFilename.c_str(), "w");
	 
	   fprintf(fp, "%f %f %f %f %f %f\n", 
		   gcpArray[i].lon, gcpArray[i].lat, gcpArray[i].rad, 
		   gcpArray[i].sigma_lon, gcpArray[i].sigma_lat, gcpArray[i].sigma_rad);
	 
	   for (int j = 0; j < numFiles-1; j++){
	     //string imgFilenameNoPath = sufix_from_filename(gcpArray[i].filename[j]);
              string imgFilenameNoPath = GetFilenameNoPath(gcpArray[i].filename[j]);
	      fprintf(fp,"%s %f %f\n", 
		      (char*)(imgFilenameNoPath.c_str()), gcpArray[i].x[j], gcpArray[i].y[j]);
	   }
	   if (numFiles > 0){
	     //string imgFilenameNoPath = sufix_from_filename(gcpArray[i].filename[numFiles-1]);
              string imgFilenameNoPath = GetFilenameNoPath(gcpArray[i].filename[numFiles-1]);
	      fprintf(fp, "%s %f %f", 
		      (char*)(imgFilenameNoPath.c_str()), gcpArray[i].x[numFiles-1], gcpArray[i].y[numFiles-1]);
	   }
	   fclose(fp);
       }
  }
   

}

//computes the min and max lon and lat of the LOLA data
BBox2 FindShotBounds(const vector<vector<LOLAShot> >& trackPts) {
  Vector<float,2> min( std::numeric_limits<float>::max(), 
                       std::numeric_limits<float>::max() );
  Vector<float,2> max( std::numeric_limits<float>::min(), 
                       std::numeric_limits<float>::min() );

  for(     unsigned int i = 0; i < trackPts.size();              ++i){
    for(   unsigned int j = 0; j < trackPts[i].size();           ++j){
      for( unsigned int k = 0; k < trackPts[i][j].LOLAPt.size(); ++k){
        float lon = trackPts[i][j].LOLAPt[k].x();
        float lat = trackPts[i][j].LOLAPt[k].y();

        if (lat < min.y() ){ min.y() = lat; }
        if (lon < min.x() ){ min.x() = lon; }
        if (lat > max.y() ){ max.y() = lat; }
        if (lon > max.x() ){ max.x() = lon; }
      }
    }
  }

  BBox2 bounds( min, max );
  return bounds;
}

Vector4 FindMinMaxLat( const vector<vector<LOLAShot> >& trackPts ) {
  BBox2 bounds = FindShotBounds( trackPts );
  Vector4 coords;
  coords(0) = bounds.min().y();
  coords(1) = bounds.max().y();
  coords(2) = bounds.min().x();
  coords(3) = bounds.max().x();
  return coords;
}

/* This is not currently being used, but may be useful in the future.
void ComputeAverageShotDistance(vector<vector<LOLAShot> >trackPts)
{
  int numValidPts = 0; 
  float avgDistance = 0.0;
  Datum moon;
  moon.set_well_known_datum("D_MOON");

  for (unsigned int i = 0; i < trackPts.size(); i++){
    for (unsigned int j = 1; j < trackPts[i].size(); j++){
      if ((trackPts[i][j].LOLAPt.size()==5) && (trackPts[i][j-1].LOLAPt.size()==5)){
         
        float lon, lat, rad;
        Vector3 lon_lat_rad;
        lon = trackPts[i][j-1].LOLAPt[0].x();
	lat = trackPts[i][j-1].LOLAPt[0].y();
	rad = trackPts[i][j-1].LOLAPt[0].z();
        lon_lat_rad(0) = lon;
        lon_lat_rad(1) = lat;
        //lon_lat_rad(2) = 1000*rad;
        //Vector3 prev_xyz = cartography::lon_lat_radius_to_xyz(lon_lat_rad);

        lon_lat_rad(2) = (rad-1737.4)*1000;
        Vector3 prev_xyz = moon.geodetic_to_cartesian(lon_lat_rad);
       
       
        lon = trackPts[i][j].LOLAPt[0].x();
	lat = trackPts[i][j].LOLAPt[0].y();
	rad = trackPts[i][j].LOLAPt[0].z();
        lon_lat_rad(0) = lon;
        lon_lat_rad(1) = lat;
        //lon_lat_rad(2) = 1000*rad;
        //Vector3 xyz = cartography::lon_lat_radius_to_xyz(lon_lat_rad);
        
        lon_lat_rad(2) = (rad-1737.4)*1000;
        Vector3 xyz = moon.geodetic_to_cartesian(lon_lat_rad);
        
	float dist_x = xyz[0]-prev_xyz[0];
        float dist_y = xyz[1]-prev_xyz[1];
        float dist_z = xyz[2]-prev_xyz[2];
        float dist = sqrt(dist_x*dist_x + dist_y*dist_y + dist_z*dist_z);
        avgDistance = avgDistance + dist;
        numValidPts = numValidPts + 1;

      }
    }
  }

  cout<<"numValidPts="<<numValidPts<<endl;  
  avgDistance = avgDistance/numValidPts;
  cout<<"avgDistance= "<<avgDistance<<endl;
}
*/

/* This is not currently being used, but may be useful in the future.
//computes the average distance from the center of the shot to its neighbors in the shot
void ComputeAverageIntraShotDistance(vector<vector<LOLAShot> >trackPts)
{
  int numValidPts = 0; 
  // float avgDistance = 0.0;
  vector<Vector3> xyzArray;
  xyzArray.resize(5);
  vector<float> distArray;
  distArray.resize(5);

  for (unsigned int i = 0; i < trackPts.size(); i++){
    for (unsigned int j = 0; j < trackPts[i].size(); j++){
      if (trackPts[i][j].LOLAPt.size()==5){
        
        for (unsigned int k = 0; k < trackPts[i][j].LOLAPt.size(); k++){
	  float lon, lat, rad;
	  Vector3 lon_lat_rad;
	  lon = trackPts[i][j].LOLAPt[k].x();
	  lat = trackPts[i][j].LOLAPt[k].y();
	  rad = trackPts[i][j].LOLAPt[k].z();
          //int s = trackPts[i][j].LOLAPt[0].s;

          //cout<<"s="<<s<<endl;

	  lon_lat_rad(0) = lon;
	  lon_lat_rad(1) = lat;
	  lon_lat_rad(2) = 1000*rad;
	  xyzArray[k] = cartography::lon_lat_radius_to_xyz(lon_lat_rad);
	}
     
        for (unsigned int k = 0; k < trackPts[i][j].LOLAPt.size(); k++){
	  float x_dist = xyzArray[0][0]-xyzArray[k][0];
	  float y_dist = xyzArray[0][1]-xyzArray[k][1];
	  float z_dist = xyzArray[0][2]-xyzArray[k][2];
	  distArray[k] = distArray[k] + sqrt(x_dist*x_dist + y_dist*y_dist + z_dist*z_dist);
	}

        numValidPts = numValidPts + 1;
        

      }
    }
  }

  cout<<"numValidPts="<<numValidPts<<endl;  
  for (unsigned int k = 0; k < distArray.size(); k++){
    distArray[k] = distArray[k]/numValidPts;
    cout<<k<<":distArray= "<<distArray[k]<<endl;
  }
 
}
*/
