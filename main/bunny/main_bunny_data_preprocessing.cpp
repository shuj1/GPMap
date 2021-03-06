#if 1
// Eigen
#include "serialization/eigen_serialization.hpp" // Eigen
// includes followings inside of it
//		- #define EIGEN_NO_DEBUG		// to speed up
//		- #define EIGEN_USE_MKL_ALL	// to use Intel Math Kernel Library
//		- #include <Eigen/Core>

// combinations
// -------------------------------------------------------------------------
// |                   Observations                  |       Process       |
// |=======================================================================|
// |   (FuncObs)  |    (DerObs)         |  (AllObs)  |       | Incremental |
// | hit points,  | virtual hit points, |  FuncObs,  | Batch |-------------|
// | empty points | surface normals     |  DerObs    |       | BCM  | iBCM |
// -------------------------------------------------------------------------

// GPMap
#include "common/common.hpp"					// combinePointCloud
#include "io/io.hpp"								// loadPointCloud, savePointCloud, loadSensorPositionList
#include "visualization/cloud_viewer.hpp"	// show
#include "features/surface_normal.hpp"		// estimateSurfaceNormals
#include "filter/filters.hpp"					// downSampling
#include "gpmap/data_partitioning.hpp"	// randomSampling
using namespace GPMap;

int main(int argc, char** argv)
{
	// [0] setting - directory
	const std::string strDataFolder				("E:/Documents/GitHub/Data/");
	const std::string strDataName					("bunny");
	const std::string strInputFolder				(strDataFolder + strDataName + "/input/");
	const std::string strIntermediateFolder	(strDataFolder + strDataName + "/intermediate/");
	create_directory(strIntermediateFolder);

	// [0] setting - observations
	const size_t NUM_OBSERVATIONS = 4; 
	const std::string strObsFileNames_[]	= {"bun000", "bun090", "bun180", "bun270"};
	const std::string strFileNameAll			=  "bunny_all";
	StringList strObsFileNameList(strObsFileNames_, strObsFileNames_ + NUM_OBSERVATIONS); 

	// [0] setting - parameters for estimating surface normals
	const bool FLAG_SERACH_BY_RADIUS = false; // SearchNearestK or SearchRadius
	const float SEARCH_RADIUS = 0.01;

	// [0] setting - removal center and radius
	const pcl::PointXYZ REMOVAL_CENTER(0.045, -0.015, 0.082);
	const float REMOVAL_RADIUS = 0.035;

	// show?
	bool fShow;
	std::cout << "Do you wish to see the results? (0/1) "; 
	std::cin >> fShow;

	//////////////////////////////////////////////////////////////////////////////////////////
	//                               Original Observations                                  //
	//////////////////////////////////////////////////////////////////////////////////////////

	// done before?
	bool fRunOriginalObservations;
	std::cout << "Do you wish to pass(0) or run(1) the orignial observations? "; 
	std::cin >> fRunOriginalObservations;

	// [1-1] Hit Points - Sequential
	PointXYZCloudPtrList hitPointCloudPtrList;
	if(fRunOriginalObservations)
	{
		loadPointCloud<pcl::PointXYZ>(hitPointCloudPtrList, strObsFileNameList, strInputFolder, ".ply");				// original ply files which are transformed in global coordinates
		savePointCloud<pcl::PointXYZ>(hitPointCloudPtrList, strObsFileNameList, strIntermediateFolder, ".pcd");
		if(fShow) show<pcl::PointXYZ>("Sequential Hit Points", hitPointCloudPtrList);
	}
	else
	{
		// it will be used for random/down sampling later
		loadPointCloud<pcl::PointXYZ>(hitPointCloudPtrList, strObsFileNameList, strIntermediateFolder, ".pcd");
	}

	// [1-2] Hit Points - All
	PointXYZCloudPtr pAllHitPointCloud(new PointXYZCloud());
	PointXYZCloudPtr pAllHitPointRemovedCloud;
	if(fRunOriginalObservations)
	{
		// accumlate points
		for(size_t i = 0; i < hitPointCloudPtrList.size(); i++)	(*pAllHitPointCloud) += (*(hitPointCloudPtrList[i]));
		savePointCloud<pcl::PointXYZ>(pAllHitPointCloud, strFileNameAll, strIntermediateFolder, ".pcd");
		if(fShow) show<pcl::PointXYZ>("All Hit Points", pAllHitPointCloud);

		// remove some points
		pAllHitPointRemovedCloud = rangeRemoval<pcl::PointXYZ>(pAllHitPointCloud, REMOVAL_CENTER, REMOVAL_RADIUS);
		savePointCloud<pcl::PointXYZ>(pAllHitPointRemovedCloud, strFileNameAll, strIntermediateFolder, "_removed.pcd");
		if(fShow) show<pcl::PointXYZ>("All Hit Points Removed", pAllHitPointRemovedCloud);
	}
	else
	{
		// it will be used for random/down sampling later
		loadPointCloud<pcl::PointXYZ>(pAllHitPointCloud, strFileNameAll, strIntermediateFolder, ".pcd");
		loadPointCloud<pcl::PointXYZ>(pAllHitPointRemovedCloud, strFileNameAll, strIntermediateFolder, "_removed.pcd");
	}

	// [2] Sensor Positions
	PointXYZVList sensorPositionList;
	loadSensorPositionList(sensorPositionList, strObsFileNameList, strInputFolder, "_camera_position.txt");
	assert(NUM_OBSERVATIONS == hitPointCloudPtrList.size() && NUM_OBSERVATIONS == sensorPositionList.size());

	if(fRunOriginalObservations)
	{
		// [3-1] Function Observations (Hit Points + Unit Ray Back Vectors) - Sequential
		PointNormalCloudPtrList funcObsCloudPtrList;
		unitRayBackVectors(hitPointCloudPtrList, sensorPositionList, funcObsCloudPtrList);
		savePointCloud<pcl::PointNormal>(funcObsCloudPtrList, strObsFileNameList, strIntermediateFolder, "_func_obs.pcd");
		//loadPointCloud<pcl::PointNormal>(funcObsCloudPtrList, strObsFileNameList, strIntermediateFolder, "_func_obs.pcd");
		if(fShow) show<pcl::PointNormal>("Sequential Unit Ray Back Vectors", funcObsCloudPtrList, 0.005);

		// [3-2] Function Observations (Hit Points + Unit Ray Back Vectors) - All
		PointNormalCloudPtr pAllFuncObs(new PointNormalCloud());
		for(size_t i = 0; i < funcObsCloudPtrList.size(); i++)	*pAllFuncObs += *funcObsCloudPtrList[i];
		savePointCloud<pcl::PointNormal>(pAllFuncObs, strFileNameAll, strIntermediateFolder, "_func_obs.pcd");
		//loadPointCloud<pcl::PointNormal>(pAllFuncObs, strFileNameAll, strIntermediateFolder, "_func_obs.pcd");
		if(fShow) show<pcl::PointNormal>("All Unit Ray Back Vectors", pAllFuncObs, 0.005);

		// remove some points
		PointNormalCloudPtr pAllFuncObsRemoved = rangeRemoval<pcl::PointNormal>(pAllFuncObs, REMOVAL_CENTER, REMOVAL_RADIUS);
		savePointCloud<pcl::PointNormal>(pAllFuncObsRemoved, strFileNameAll, strIntermediateFolder, "_func_obs_removed.pcd");
		//loadPointCloud<pcl::PointNormal>(pAllFuncObsRemoved, strFileNameAll, strIntermediateFolder, "_func_obs_removed.pcd");
		if(fShow) show<pcl::PointNormal>("All Unit Ray Back Vectors Removed", pAllFuncObsRemoved, 0.005);

		// [4-1] Derivative Observations (Virtual Hit Points + Surface Normal Vectors) - Sequential
		PointNormalCloudPtrList derObsCloudPtrList;
		//estimateSurfaceNormals<ByNearestNeighbors>(hitPointCloudPtrList, sensorPositionList, FLAG_SERACH_BY_RADIUS, SEARCH_RADIUS, derObsCloudPtrList);
		estimateSurfaceNormals<ByMovingLeastSquares>(hitPointCloudPtrList, sensorPositionList, FLAG_SERACH_BY_RADIUS, SEARCH_RADIUS, derObsCloudPtrList);
		savePointCloud<pcl::PointNormal>(derObsCloudPtrList, strObsFileNameList, strIntermediateFolder, "_der_obs.pcd");
		//loadPointCloud<pcl::PointNormal>(derObsCloudPtrList, strObsFileNameList, strIntermediateFolder, "_der_obs.pcd");
		if(fShow) show<pcl::PointNormal>("Sequential Surface Normals", derObsCloudPtrList, 0.005);

		// [4-2] Derivative Observations (Virtual Hit Points + Surface Normal Vectors) - All
		PointNormalCloudPtr pAllDerObs(new PointNormalCloud());
		for(size_t i = 0; i < derObsCloudPtrList.size(); i++)	(*pAllDerObs) += (*(derObsCloudPtrList[i]));
		savePointCloud<pcl::PointNormal>(pAllDerObs, strFileNameAll, strIntermediateFolder, "_der_obs.pcd");
		//loadPointCloud<pcl::PointNormal>(pAllDerObs, strFileNameAll, strIntermediateFolder, "_der_obs.pcd");
		if(fShow) show<pcl::PointNormal>("All Surface Normals", pAllDerObs, 0.005);

		// remove some points
		PointNormalCloudPtr pAllDerObsRemoved = rangeRemoval<pcl::PointNormal>(pAllDerObs, REMOVAL_CENTER, REMOVAL_RADIUS);
		savePointCloud<pcl::PointNormal>(pAllDerObsRemoved, strFileNameAll, strIntermediateFolder, "_der_obs_removed.pcd");
		//loadPointCloud<pcl::PointNormal>(pAllDerObsRemoved, strFileNameAll, strIntermediateFolder, "_der_obs_removed.pcd");
		if(fShow) show<pcl::PointNormal>("All Surface Normals Removed", pAllDerObsRemoved, 0.005);

	}

	//////////////////////////////////////////////////////////////////////////////////////////
	//                           Down Sampled Observations                                  //
	//////////////////////////////////////////////////////////////////////////////////////////

	bool fOctreeDownSampling;
	std::cout << "Random sampling(0) or octree-based down sampling(1)? ";
	std::cin >> fOctreeDownSampling;

	// sub-folder for samples
	std::string strIntermediateSampleFolder;
	float param;
	if(fOctreeDownSampling)
	{
		// leaf size
		std::cout << "Down sampling leaf size: ";
		std::cin >> param; // 0.001(50%), 0.002(20%), 0.003(10%)

		// sub-folder
		std::stringstream ss;
		ss << strIntermediateFolder << "down_sampling_" << param << "/";
		strIntermediateSampleFolder = ss.str();
	}
	else
	{
		// sampling ratio
		std::cout << "Random sampling ratio: ";
		std::cin >> param;	// 0.5, 0.3, 0.2, 0.1

		// sub-folder
		std::stringstream ss;
		ss << strIntermediateFolder << "random_sampling_" << param << "/";
		strIntermediateSampleFolder = ss.str();
	}
	create_directory(strIntermediateSampleFolder);

	// [1-1] Hit Points - Sequential - Down Sampling
	PointXYZCloudPtrList sampledHitPointCloudPtrList;
	if(fOctreeDownSampling)	downSampling  <pcl::PointXYZ>(hitPointCloudPtrList, param, sampledHitPointCloudPtrList);
	else							randomSampling<pcl::PointXYZ>(hitPointCloudPtrList, param, sampledHitPointCloudPtrList);
	savePointCloud<pcl::PointXYZ>(sampledHitPointCloudPtrList, strObsFileNameList, strIntermediateSampleFolder, ".pcd");
	//loadPointCloud<pcl::PointXYZ>(sampledHitPointCloudPtrList, strObsFileNameList, strIntermediateSampleFolder, ".pcd");
	if(fShow) show<pcl::PointXYZ>("Sequential Sampled Hit Points", sampledHitPointCloudPtrList);

	// sampling rate
	std::cout << "Hit Points - Sequential - Down Sampling" << std::endl;
	int totalNumPoints(0), totalNumSampledPoints(0);
	for(size_t i = 0; i < sampledHitPointCloudPtrList.size(); i++)
	{
		totalNumPoints				+= hitPointCloudPtrList[i]->points.size();
		totalNumSampledPoints	+= sampledHitPointCloudPtrList[i]->points.size();
		std::cout << "[" << i << "] sampling rate: " 
					 << sampledHitPointCloudPtrList[i]->points.size() << " / "
					 << hitPointCloudPtrList[i]->points.size() << " = " 
					 << 100.f * static_cast<float>(sampledHitPointCloudPtrList[i]->points.size())/static_cast<float>(hitPointCloudPtrList[i]->points.size()) << "%" << std::endl;
	}
	std::cout << "Total sampling rate: " 
				 << totalNumSampledPoints << " / "
				 << totalNumPoints << " = " 
				 << 100.f * static_cast<float>(totalNumSampledPoints)/static_cast<float>(totalNumPoints) << "%" << std::endl;

	// [1-2] Hit Points - All - Down Sampling
	PointXYZCloudPtr pAllSampledHitPointCloud(new PointXYZCloud());
	for(size_t i = 0; i < sampledHitPointCloudPtrList.size(); i++)	(*pAllSampledHitPointCloud) += (*(sampledHitPointCloudPtrList[i]));
	savePointCloud<pcl::PointXYZ>(pAllSampledHitPointCloud, strFileNameAll, strIntermediateSampleFolder, ".pcd");
	//loadPointCloud<pcl::PointXYZ>(pAllSampledHitPointCloud, strFileNameAll, strIntermediateSampleFolder, ".pcd");
	if(fShow) show<pcl::PointXYZ>("All Sampled Hit Points", pAllSampledHitPointCloud);

	// remove some points
	PointXYZCloudPtr pAllSampledHitPointRemovedCloud = rangeRemoval<pcl::PointXYZ>(pAllSampledHitPointCloud, REMOVAL_CENTER, REMOVAL_RADIUS);
	savePointCloud<pcl::PointXYZ>(pAllSampledHitPointRemovedCloud, strFileNameAll, strIntermediateSampleFolder, "_removed.pcd");
	if(fShow) show<pcl::PointXYZ>("All Sampled Hit Points Removed", pAllSampledHitPointRemovedCloud);

	// [3-1] Function Observations (Hit Points + Unit Ray Back Vectors) - Sequential - Down Sampling
	PointNormalCloudPtrList sampledFuncObsCloudPtrList;
	unitRayBackVectors(sampledHitPointCloudPtrList, sensorPositionList, sampledFuncObsCloudPtrList);
	savePointCloud<pcl::PointNormal>(sampledFuncObsCloudPtrList, strObsFileNameList, strIntermediateSampleFolder, "_func_obs.pcd");
	//loadPointCloud<pcl::PointNormal>(sampledFuncObsCloudPtrList, strObsFileNameList, strIntermediateSampleFolder, "_func_obs.pcd");
	if(fShow) show<pcl::PointNormal>("Sequential Sampled Unit Ray Back Vectors", sampledFuncObsCloudPtrList, 0.005);

	// [3-2] Function Observations (Hit Points + Unit Ray Back Vectors) - All - Down Sampling
	PointNormalCloudPtr pAllSampledFuncObs(new PointNormalCloud());
	for(size_t i = 0; i < sampledFuncObsCloudPtrList.size(); i++)	*pAllSampledFuncObs += *sampledFuncObsCloudPtrList[i];
	savePointCloud<pcl::PointNormal>(pAllSampledFuncObs, strFileNameAll, strIntermediateSampleFolder, "_func_obs.pcd");
	//loadPointCloud<pcl::PointNormal>(pAllSampledFuncObs, strFileNameAll, strIntermediateSampleFolder, "_func_obs.pcd");
	if(fShow) show<pcl::PointNormal>("All Sampled Unit Ray Back Vectors", pAllSampledFuncObs, 0.005);

	// remove some points
	PointNormalCloudPtr pAllSampledFuncObsRemoved = rangeRemoval<pcl::PointNormal>(pAllSampledFuncObs, REMOVAL_CENTER, REMOVAL_RADIUS);
	savePointCloud<pcl::PointNormal>(pAllSampledFuncObsRemoved, strFileNameAll, strIntermediateSampleFolder, "_func_obs_removed.pcd");
	//loadPointCloud<pcl::PointNormal>(pAllSampledFuncObsRemoved, strFileNameAll, strIntermediateSampleFolder, "_func_obs_removed.pcd");
	if(fShow) show<pcl::PointNormal>("All Sampled Unit Ray Back Vectors Removed", pAllSampledFuncObsRemoved, 0.005);

	// downsampling
	std::cout << "Down sampling leaf size: ";
	std::cin >> param; // 0.001(50%), 0.002(20%), 0.003(10%)
	std::stringstream ss;
	ss << "_func_obs_removed_downsampled_" << param << ".pcd";
	PointNormalCloudPtr pAllSampledFuncObsRemovedDownSampled = downSampling<pcl::PointNormal>(pAllSampledFuncObsRemoved, param);
	std::cout << "Sampling rate: " << pAllSampledFuncObsRemovedDownSampled->points.size() << " / "
											 << pAllSampledFuncObsRemoved->points.size() << " = " 
											 << 100.f * static_cast<float>(pAllSampledFuncObsRemovedDownSampled->points.size())/static_cast<float>(pAllSampledFuncObsRemoved->points.size()) << "%" << std::endl;
	savePointCloud<pcl::PointNormal>(pAllSampledFuncObsRemovedDownSampled, strFileNameAll, strIntermediateSampleFolder, ss.str());
	//loadPointCloud<pcl::PointNormal>(pAllSampledFuncObsRemovedDownSampled, strFileNameAll, strIntermediateSampleFolder, ss.str());
	//if(fShow) show<pcl::PointNormal>("All Sampled Unit Ray Back Vectors Removed Downsampled", pAllSampledFuncObsRemovedDownSampled, 0.005);
	show<pcl::PointNormal>("All Sampled Unit Ray Back Vectors Removed Downsampled", pAllSampledFuncObsRemovedDownSampled, 0.005);

	// [4-1] Derivative Observations (Virtual Hit Points + Surface Normal Vectors) - Sequential - Down Sampling
	PointNormalCloudPtrList sampledDerObsCloudPtrList;
	//estimateSurfaceNormals<ByNearestNeighbors>(sampledHitPointCloudPtrList, sensorPositionList, FLAG_SERACH_BY_RADIUS, SEARCH_RADIUS, sampledDerObsCloudPtrList);
	estimateSurfaceNormals<ByMovingLeastSquares>(sampledHitPointCloudPtrList, sensorPositionList, FLAG_SERACH_BY_RADIUS, SEARCH_RADIUS, sampledDerObsCloudPtrList);
	savePointCloud<pcl::PointNormal>(sampledDerObsCloudPtrList, strObsFileNameList, strIntermediateSampleFolder, "_der_obs.pcd");
	//loadPointCloud<pcl::PointNormal>(sampledDerObsCloudPtrList, strObsFileNameList, strIntermediateSampleFolder, "_der_obs.pcd");
	if(fShow) show<pcl::PointNormal>("Sequential Sampled Surface Normals", sampledDerObsCloudPtrList, 0.005);

	// [4-2] Derivative Observations (Virtual Hit Points + Surface Normal Vectors) - All - Down Sampling
	PointNormalCloudPtr pAllSampledDerObs(new PointNormalCloud());
	for(size_t i = 0; i < sampledDerObsCloudPtrList.size(); i++)	(*pAllSampledDerObs) += (*(sampledDerObsCloudPtrList[i]));
	savePointCloud<pcl::PointNormal>(pAllSampledDerObs, strFileNameAll, strIntermediateSampleFolder, "_der_obs.pcd");
	//loadPointCloud<pcl::PointNormal>(pAllSampledDerObs, strFileNameAll, strIntermediateSampleFolder, "_der_obs.pcd");
	if(fShow) show<pcl::PointNormal>("All Sampled Surface Normals", pAllSampledDerObs, 0.005);

	// remove some points
	PointNormalCloudPtr pAllSampledDerObsRemoved = rangeRemoval<pcl::PointNormal>(pAllSampledDerObs, REMOVAL_CENTER, REMOVAL_RADIUS);
	savePointCloud<pcl::PointNormal>(pAllSampledDerObsRemoved, strFileNameAll, strIntermediateSampleFolder, "_der_obs_removed.pcd");
	//loadPointCloud<pcl::PointNormal>(pAllSampledDerObsRemoved, strFileNameAll, strIntermediateSampleFolder, "_der_obs_removed.pcd");
	if(fShow) show<pcl::PointNormal>("All Sampled Surface Normals Removed", pAllSampledDerObsRemoved, 0.005);

	// downsampling
	std::stringstream ss2;
	ss2 << "_der_obs_removed_downsampled_" << param << ".pcd";
	PointNormalCloudPtr pAllSampledDerObsRemovedDownSampled = downSampling<pcl::PointNormal>(pAllSampledDerObsRemoved, param);
	std::cout << "Sampling rate: " << pAllSampledDerObsRemovedDownSampled->points.size() << " / "
											 << pAllSampledDerObsRemoved->points.size() << " = " 
											 << 100.f * static_cast<float>(pAllSampledDerObsRemovedDownSampled->points.size())/static_cast<float>(pAllSampledDerObsRemoved->points.size()) << "%" << std::endl;
	savePointCloud<pcl::PointNormal>(pAllSampledDerObsRemovedDownSampled, strFileNameAll, strIntermediateSampleFolder, ss2.str());
	//loadPointCloud<pcl::PointNormal>(pAllSampledDerObsRemovedDownSampled, strFileNameAll, strIntermediateSampleFolder, ss2.str());
	//if(fShow) show<pcl::PointNormal>("All Sampled Surface Normals Removed Downsampled", pAllSampledDerObsRemovedDownSampled, 0.005);
	show<pcl::PointNormal>("All Sampled Surface Normals Removed Downsampled", pAllSampledDerObsRemovedDownSampled, 0.005);

	// [5] All (Func + Der) Observations
	//PointNormalCloudPtrList allObsCloudPtrList;
	//combinePointCloud<pcl::PointNormal>(funcObsCloudPtrList, derObsCloudPtrList, allObsCloudPtrList);
	//savePointCloud<pcl::PointNormal>(allObsCloudPtrList, strObsFileNameList, strIntermediateFolder, "_all_obs.pcd");
	//loadPointCloud<pcl::PointNormal>(allObsCloudPtrList, strObsFileNameList, strIntermediateFolder, "_all_obs.pcd");
	//if(fShow) show<pcl::PointNormal>("All Observation List", allObsCloudPtrList, 0.005);

	//PointNormalCloudPtr pAllObs(new PointNormalCloud());
	//*pAllObs += *pAllFuncObs;
	//*pAllObs += *pAllDerObs;
	//savePointCloud<pcl::PointNormal>(pAllObs, strFileNameAll, strIntermediateFolder, "_all_obs.pcd");
	//loadPointCloud<pcl::PointNormal>(pAllObs, strFileNameAll, strIntermediateFolder, "_all_obs.pcd");
	//if(fShow) show<pcl::PointNormal>("All Observations", pAllDerObs, 0.005, 0.001);

	system("pause");
}

#endif