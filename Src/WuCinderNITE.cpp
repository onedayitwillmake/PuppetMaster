/*
 * WuCinderNITE.cpp
 *
 *  Created on: Jul 23, 2011
 *      Author: guojianwu
 */

#include "WuCinderNITE.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "gl.h"

#include <XnStatusCodes.h>
#include <XnTypes.h>

XnUInt32 mNITENumNITEUserColors = 10;
XnFloat mNITEUserColors[][3] = {
	{0, 0, 1},
	{1, 0.3,0.2},
	{0,1,0},
	{1,1,0},
	{1,0,0},
	{1,.5,0},
	{.5,1,0},
	{0,.5,1},
	{.5,0,1},
	{1,1,.5},
	{1,1,1}
};


WuCinderNITE* WuCinderNITE::mInstance = NULL;
WuCinderNITE* WuCinderNITE::getInstance()
{
	if (mInstance == NULL) {
		mInstance = new WuCinderNITE();
	}
	return mInstance;
}

WuCinderNITE::WuCinderNITE() {
	useSingleCalibrationMode = true;
	mNeedPoseForCalibration = false;
	mIsCalibrated = false;
	mRunUpdates = false;
	mNumUsers = 0;
}

WuCinderNITE::~WuCinderNITE() {
	unregisterCallbacks();
	if (mRunUpdates) {
		stopUpdating();
	}
	mContext.Shutdown();
	mContext.Release();
	mDepthGen.Release();
	mUserGen.Release();
	mImageGen.Release();
	mSceneAnalyzer.Release();
}

void WuCinderNITE::shutdown()
{
	if (mInstance != NULL) {
		delete mInstance;
		mInstance = NULL;
	}
}

void WuCinderNITE::setup(string onipath)
{
	mMapMode.nXRes = 0;
	XnStatus status = XN_STATUS_OK;
	xn::EnumerationErrors errors;
	status = mContext.Init();
	CHECK_RC(status, "Init", true);

	status = mContext.OpenFileRecording(onipath.c_str());
	CHECK_RC(status, "Recording", true);

	status = mContext.FindExistingNode(XN_NODE_TYPE_DEPTH, mDepthGen);
	if (status == XN_STATUS_OK) {
		mUseDepthMap = true;

		status = mDepthGen.GetMapOutputMode(mMapMode);
		CHECK_RC(status, "Retrieving XnMapOutputMode", true);

		mDepthSurface = ci::Surface8u(mMapMode.nXRes, mMapMode.nYRes, false);
		mDrawArea = ci::Area(0, 0, mMapMode.nXRes, mMapMode.nYRes);
		maxDepth = mDepthGen.GetDeviceMaxDepth();
	} else {
		mUseDepthMap = false;
		maxDepth = 0;
	}

	status = mContext.FindExistingNode(XN_NODE_TYPE_IMAGE, mImageGen);
	if (status != XN_STATUS_OK) {
		mUseColorImage = true;
		status = mImageGen.Create(mContext);
		CHECK_RC(status, "Image Gen", true);

		if (mMapMode.nXRes == 0) {
			status = mImageGen.GetMapOutputMode(mMapMode);
			CHECK_RC(status, "Retrieving XnMapOutputMode", true);
		}

		mImageSurface = ci::Surface8u(mMapMode.nXRes, mMapMode.nYRes, false);
	} else {
		mUseColorImage = false;
	}

	status = mContext.FindExistingNode(XN_NODE_TYPE_USER, mUserGen);
	if (status != XN_STATUS_OK) {
		status = mUserGen.Create(mContext);
		CHECK_RC(status, "User Gen", true);
	}
	mUserGen.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

	if (!mUserGen.IsCapabilitySupported((const char*)XN_CAPABILITY_SKELETON)) {
		ci::app::console() << "Supplied user generator doesn't support skeleton" << endl;
		exit(-1);
	}

	status = mContext.FindExistingNode(XN_NODE_TYPE_SCENE, mSceneAnalyzer);
	if (status != XN_STATUS_OK) {
		status = mSceneAnalyzer.Create(mContext);
		CHECK_RC(status, "Scene Analyzer", true);
	}

	registerCallbacks();
}
void WuCinderNITE::setup(string xmlpath, XnMapOutputMode mapMode, bool useDepthMap, bool useColorImage)
{
	mMapMode = mapMode;
	mUseDepthMap = useDepthMap;
	mUseColorImage = useColorImage;
	mDrawArea = ci::Area(0, 0, mMapMode.nXRes, mMapMode.nYRes);

	XnStatus status = XN_STATUS_OK;
	xn::EnumerationErrors errors;
	status = mContext.InitFromXmlFile(xmlpath.c_str(), &errors);
	CHECK_RC(status, "Init", true);


	if (mUseDepthMap) {
		status = mContext.FindExistingNode(XN_NODE_TYPE_DEPTH, mDepthGen);
		if (status != XN_STATUS_OK) {
			status = mDepthGen.Create(mContext);
			CHECK_RC(status, "Depth Creating", true);
		}
		status = mDepthGen.SetMapOutputMode(mMapMode);
		CHECK_RC(status, "Depth Settings", true);
		mDepthSurface = ci::Surface8u(mMapMode.nXRes, mMapMode.nYRes, false);
		maxDepth = mDepthGen.GetDeviceMaxDepth();
	} else {
		maxDepth = 0;
	}

	if (mUseColorImage) {
		status = mContext.FindExistingNode(XN_NODE_TYPE_IMAGE, mImageGen);
		if (status != XN_STATUS_OK) {
			status = mImageGen.Create(mContext);
			CHECK_RC(status, "Image Gen", true);
		}
		status = mImageGen.SetMapOutputMode(mMapMode);
		CHECK_RC(status, "Image Settings", true);
		mImageSurface = ci::Surface8u(mMapMode.nXRes, mMapMode.nYRes, false);
	}

	status = mContext.FindExistingNode(XN_NODE_TYPE_USER, mUserGen);
	if (status != XN_STATUS_OK) {
		status = mUserGen.Create(mContext);
		CHECK_RC(status, "User Gen", true);
	}
	mUserGen.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

	if (!mUserGen.IsCapabilitySupported((const char*)XN_CAPABILITY_SKELETON)) {
		ci::app::console() << "Supplied user generator doesn't support skeleton" << endl;
		exit(-1);
	}

	status = mContext.FindExistingNode(XN_NODE_TYPE_SCENE, mSceneAnalyzer);
	if (status != XN_STATUS_OK) {
		status = mSceneAnalyzer.Create(mContext);
		CHECK_RC(status, "Scene Analyzer", true);
	}

	registerCallbacks();
}


void WuCinderNITE::startUpdating()
{
	if (mRunUpdates) {
		return;
	}
	mRunUpdates = true;
	mContext.StartGeneratingAll();

	mThread = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&WuCinderNITE::update, this)));
}

void WuCinderNITE::stopUpdating()
{
	if (!mRunUpdates) {
		return;
	}
	mRunUpdates = false;

	mThread->join();
	mContext.StopGeneratingAll();
}

void WuCinderNITE::update()
{
	while (mRunUpdates) {
		XnStatus status = XN_STATUS_OK;

		status = mContext.WaitAndUpdateAll();
		if( status != XN_STATUS_OK ) {
			ci::app::console() << "no update" << endl;
			continue;
		}

		if( !mUserGen ) {
			ci::app::console() << "No user generator" << endl;
			return;
		}

		status = mContext.FindExistingNode(XN_NODE_TYPE_DEPTH, mDepthGen);
		if( status != XN_STATUS_OK ) {
			ci::app::console() << xnGetStatusString(status) << endl;
			continue;
		}
		mSceneAnalyzer.GetFloor(mFloor);

		mUserGen.GetUserPixels(0, mSceneMeta);
		if (mUseDepthMap) {
			mDepthGen.GetMetaData(mDepthMeta);
			updateDepthSurface();
		}
		if (mUseColorImage) {
			mImageGen.GetMetaData(mImageMeta);
			updateImageSurface();
		}
	}
}

void WuCinderNITE::updateDepthSurface()
{
	int w = mDepthMeta.XRes();
	int h = mDepthMeta.YRes();
	unsigned int numPoints = 0;
	unsigned int nValue = 0;
	unsigned int nHistValue = 0;

	const XnDepthPixel* pDepth = mDepthMeta.Data();
	const XnLabel* pLabels = mSceneMeta.Data();
	bool hasSceneData = pLabels != 0;

	// set all items of the array to 0
	memset(mDepthHistogram, 0, MAX_DEPTH*sizeof(float));

	// histogram logic from NiSimpleViewer.cpp
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			if (*pDepth != 0) {
				mDepthHistogram[*pDepth]++;
				numPoints++;
			}
			pDepth++;
		}
	}
	for (int nIndex=1; nIndex < MAX_DEPTH; nIndex++) {
		mDepthHistogram[nIndex] += mDepthHistogram[nIndex-1];
	}
	if (numPoints) {
		for (int nIndex = 1; nIndex < MAX_DEPTH; nIndex++) {
			mDepthHistogram[nIndex] = (unsigned int)(256 * (1.0f - (mDepthHistogram[nIndex] / numPoints)));
		}
	}

	pDepth = mDepthMeta.Data();
	ci::Surface::Iter iter = mDepthSurface.getIter( mDrawArea );
	while( iter.line() ) {
		while( iter.pixel() ) {
			iter.r() = 0;
			iter.g() = 0;
			iter.b() = 0;

			if ( hasSceneData && *pLabels != 0) {
				nValue = *pDepth;
				XnLabel label = *pLabels;
				XnUInt32 nColorID = label % mNITENumNITEUserColors;
				if (label == 0) {
					nColorID = mNITENumNITEUserColors;
				}
				if (nValue != 0) {
					nHistValue = mDepthHistogram[nValue];

					iter.r() = nHistValue * mNITEUserColors[nColorID][0];
					iter.g() = nHistValue * mNITEUserColors[nColorID][1];
					iter.b() = nHistValue * mNITEUserColors[nColorID][2];
				}
			} else {
				nValue = *pDepth;
				if (nValue != 0) {
					nHistValue = mDepthHistogram[nValue];
					iter.r() = nHistValue;
					iter.g() = nHistValue;
					iter.b() = nHistValue;
				}
			}
			pDepth++;
			pLabels++;
		}
	}
}

void WuCinderNITE::updateImageSurface()
{
	int w = mImageMeta.XRes();
	const XnRGB24Pixel* pImage = mImageMeta.RGB24Data();
	ci::Surface::Iter iter = mImageSurface.getIter( mDrawArea );
	while( iter.line() ) {
		pImage += w;
		while( iter.pixel() ) {
			iter.r() = pImage->nRed;
			iter.g() = pImage->nGreen;
			iter.b() = pImage->nBlue;
			pImage--; // image is flipped, let's read backwards
		}
		pImage += w;
	}
}

ci::Surface8u WuCinderNITE::getDepthSurface()
{
	return mDepthSurface;
}

ci::Surface8u WuCinderNITE::getImageSurface()
{
	return mImageSurface;
}

XnMapOutputMode WuCinderNITE::getMapMode()
{
	return mMapMode;
}

void WuCinderNITE::renderDepthMap(ci::Area area)
{
	if (mUseDepthMap) {
		ci::gl::pushMatrices();
		ci::gl::draw( ci::gl::Texture(mDepthSurface, ci::gl::Texture::Format::Format() ), area );
		ci::gl::popMatrices();
	}
}

void WuCinderNITE::renderColor(ci::Area area)
{
	if(mUseColorImage) {
		ci::gl::draw( ci::gl::Texture(mImageSurface, ci::gl::Texture::Format::Format() ), area );
	}
}

void WuCinderNITE::renderSkeleton()
{
	for (int nUser = 0; nUser < mNumUsers; nUser++) {
		if (mUserGen.GetSkeletonCap().IsTracking(mUsers[nUser])) {

			glLineWidth(3);
			ci::gl::color(1-mNITEUserColors[mUsers[nUser]%mNITENumNITEUserColors][0],
								  1-mNITEUserColors[mUsers[nUser]%mNITENumNITEUserColors][1],
								  1-mNITEUserColors[mUsers[nUser]%mNITENumNITEUserColors][2], 1);

			// HEAD TO NECK
			renderLimb(mUsers[nUser], XN_SKEL_HEAD, XN_SKEL_NECK);

			// Left Arm
			renderLimb(mUsers[nUser], XN_SKEL_NECK, XN_SKEL_LEFT_SHOULDER);
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_ELBOW);
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_HAND);
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_HAND, XN_SKEL_LEFT_FINGERTIP);

			// RIGHT ARM
			renderLimb(mUsers[nUser], XN_SKEL_NECK, XN_SKEL_RIGHT_SHOULDER);
			renderLimb(mUsers[nUser], XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_ELBOW);
			renderLimb(mUsers[nUser], XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_HAND);
			renderLimb(mUsers[nUser], XN_SKEL_RIGHT_HAND, XN_SKEL_RIGHT_FINGERTIP);
			// TORSO
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_SHOULDER, XN_SKEL_TORSO);
			renderLimb(mUsers[nUser], XN_SKEL_RIGHT_SHOULDER, XN_SKEL_TORSO);

			// LEFT LEG
			renderLimb(mUsers[nUser], XN_SKEL_TORSO, XN_SKEL_LEFT_HIP);
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_HIP, XN_SKEL_LEFT_KNEE);
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_KNEE, XN_SKEL_LEFT_FOOT);

			// RIGHT LEG
			renderLimb(mUsers[nUser], XN_SKEL_TORSO, XN_SKEL_RIGHT_HIP);
			renderLimb(mUsers[nUser], XN_SKEL_RIGHT_HIP, XN_SKEL_RIGHT_KNEE);
			renderLimb(mUsers[nUser], XN_SKEL_RIGHT_KNEE, XN_SKEL_RIGHT_FOOT);
			// PELVIS
			renderLimb(mUsers[nUser], XN_SKEL_LEFT_HIP, XN_SKEL_RIGHT_HIP);
			glLineWidth(1);
		}
		ci::gl::color(1, 1, 1, 1);
	}
}

void WuCinderNITE::renderLimb(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2, float confidence)
{
	if (!mUserGen.GetSkeletonCap().IsCalibrated(player)) {
		ci::app::console() << player << ":not calibrated!" << endl;
		return;
	}
	if (!mUserGen.GetSkeletonCap().IsTracking(player)) {
		ci::app::console() << player << ":not tracked!" << endl;
		return;
	}

	XnSkeletonJointPosition joint1, joint2;
	mUserGen.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint1, joint1);
	mUserGen.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint2, joint2);
	if (joint1.fConfidence < confidence || joint2.fConfidence < confidence) {
		return;
	}
	if (true) {
		// 3D - arbitrary scale
		ci::gl::drawLine(ci::Vec3f(joint1.position.X, joint1.position.Y, joint1.position.Z),
				ci::Vec3f(joint2.position.X, joint2.position.Y, joint2.position.Z));

	} else {
		// 2D
		XnPoint3D pt[2];
		pt[0] = joint1.position;
		pt[1] = joint2.position;
		mDepthGen.ConvertRealWorldToProjective(2, pt, pt);
		ci::gl::drawLine(ci::Vec2f(pt[0].X, pt[0].Y), ci::Vec2f(pt[1].X, pt[1].Y));
	}
}

void WuCinderNITE::findUsers() {
	mNumUsers = 15;
	mUserGen.GetUsers(mUsers, mNumUsers);
}

void WuCinderNITE::registerCallbacks()
{
	XnStatus status = XN_STATUS_OK;
	status = mUserGen.RegisterUserCallbacks(mInstance->CB_NewUser, mInstance->CB_LostUser, NULL, hUserCBs);
	CHECK_RC(status, "User callbacks", true);

	status = mUserGen.GetSkeletonCap().RegisterCalibrationCallbacks(mInstance->CB_CalibrationStart, mInstance->CB_CalibrationEnd, NULL, hCalibrationPhasesCBs);
	CHECK_RC(status, "Calibrations callbacks 1", true);

	status = mUserGen.GetSkeletonCap().RegisterToCalibrationComplete(mInstance->CB_CalibrationComplete, NULL, hCalibrationCompleteCBs);
	CHECK_RC(status, "Calibrations callbacks 2", true);

	if (mUserGen.GetSkeletonCap().NeedPoseForCalibration()) {
		mNeedPoseForCalibration = true;
		if (!mUserGen.IsCapabilitySupported((const char*)XN_CAPABILITY_POSE_DETECTION)) {
			ci::app::console() << "Need pose for calibration but device does not support it" << endl;
			exit(-1);
		}

		status = mUserGen.GetPoseDetectionCap().RegisterToPoseCallbacks(mInstance->CB_PoseDetected, NULL, NULL, hPoseCBs);
		CHECK_RC(status, "Pose callbacks", true);

		mUserGen.GetSkeletonCap().GetCalibrationPose(mCalibrationPose);
	}
}

void WuCinderNITE::unregisterCallbacks()
{
	mUserGen.UnregisterUserCallbacks(hUserCBs);
	mUserGen.GetSkeletonCap().UnregisterCalibrationCallbacks(hCalibrationPhasesCBs);
	mUserGen.GetSkeletonCap().UnregisterFromCalibrationComplete(hCalibrationCompleteCBs);
	if (mUserGen.GetSkeletonCap().NeedPoseForCalibration()) {
		mUserGen.GetPoseDetectionCap().UnregisterFromPoseCallbacks(hPoseCBs);
	}
}

void XN_CALLBACK_TYPE WuCinderNITE::CB_NewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie)
{
	ci::app::console() << "new user " << nId << endl;
	if (mInstance->mNeedPoseForCalibration) {
		if (mInstance->mIsCalibrated) {
			mInstance->mUserGen.GetSkeletonCap().LoadCalibrationData(nId, 0);
			mInstance->mUserGen.GetSkeletonCap().StartTracking(nId);
		} else {
			mInstance->mUserGen.GetPoseDetectionCap().StartPoseDetection(mInstance->mCalibrationPose, nId);
		}
	} else {
		mInstance->mUserGen.GetSkeletonCap().RequestCalibration(nId, TRUE);
	}
	mInstance->findUsers();
	mInstance->signalNewUser(nId);
}
void XN_CALLBACK_TYPE WuCinderNITE::CB_LostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie)
{
	ci::app::console() << "lost user " << nId << endl;
	mInstance->findUsers();
	mInstance->signalLostUser(nId);
}
void XN_CALLBACK_TYPE WuCinderNITE::CB_CalibrationStart(xn::SkeletonCapability& capability, XnUserID nId, void* pCookie)
{
	// nothing
}
void XN_CALLBACK_TYPE WuCinderNITE::CB_CalibrationEnd(xn::SkeletonCapability& capability, XnUserID nId, XnBool bSuccess, void* pCookie)
{
	ci::app::console() << "CB_CalibrationEnd " << nId << (bSuccess ? " success" : " failed")  << endl;
	if (!bSuccess) {
		if (mInstance->mNeedPoseForCalibration) {
			mInstance->mUserGen.GetPoseDetectionCap().StartPoseDetection(mInstance->mCalibrationPose, nId);
		} else {
			mInstance->mUserGen.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}
void XN_CALLBACK_TYPE WuCinderNITE::CB_CalibrationComplete(xn::SkeletonCapability& skeleton, XnUserID nId, XnCalibrationStatus eStatus, void* cxt)
{
	ci::app::console() << "calibration completed for user " << nId << (eStatus == XN_CALIBRATION_STATUS_OK ? " success" : " failed") << endl;
	if (eStatus == XN_CALIBRATION_STATUS_OK) {
		if (!mInstance->mIsCalibrated) {
			if (mInstance->useSingleCalibrationMode) {
				mInstance->mIsCalibrated = TRUE;
				mInstance->mUserGen.GetSkeletonCap().SaveCalibrationData(nId, 0);
			}
			mInstance->mUserGen.GetSkeletonCap().StartTracking(nId);
			mInstance->mUserGen.GetSkeletonCap().SetSmoothing(0.5f);
		}
		mInstance->mUserGen.GetPoseDetectionCap().StopPoseDetection(nId);
		mInstance->findUsers();
	}
}
void XN_CALLBACK_TYPE WuCinderNITE::CB_PoseDetected(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID nId, void* pCookie)
{
	ci::app::console() << "pose detected for user " << nId << endl;
	mInstance->mUserGen.GetPoseDetectionCap().StopPoseDetection(nId);
	mInstance->mUserGen.GetSkeletonCap().RequestCalibration(nId, TRUE);
}

