///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, Tadas Baltrusaitis, all rights reserved.
//
// ACADEMIC OR NON-PROFIT ORGANIZATION NONCOMMERCIAL RESEARCH USE ONLY
//
// BY USING OR DOWNLOADING THE SOFTWARE, YOU ARE AGREEING TO THE TERMS OF THIS LICENSE AGREEMENT.  
// IF YOU DO NOT AGREE WITH THESE TERMS, YOU MAY NOT USE OR DOWNLOAD THE SOFTWARE.
//
// License can be found in OpenFace-license.txt
//
//     * Any publications arising from the use of this software, including but
//       not limited to academic journal and conference publications, technical
//       reports and manuals, must cite at least one of the following works:
//
//       OpenFace: an open source facial behavior analysis toolkit
//       Tadas Baltru�aitis, Peter Robinson, and Louis-Philippe Morency
//       in IEEE Winter Conference on Applications of Computer Vision, 2016  
//
//       Rendering of Eyes for Eye-Shape Registration and Gaze Estimation
//       Erroll Wood, Tadas Baltru�aitis, Xucong Zhang, Yusuke Sugano, Peter Robinson, and Andreas Bulling 
//       in IEEE International. Conference on Computer Vision (ICCV),  2015 
//
//       Cross-dataset learning and person-speci?c normalisation for automatic Action Unit detection
//       Tadas Baltru�aitis, Marwa Mahmoud, and Peter Robinson 
//       in Facial Expression Recognition and Analysis Challenge, 
//       IEEE International Conference on Automatic Face and Gesture Recognition, 2015 
//
//       Constrained Local Neural Fields for robust facial landmark detection in the wild.
//       Tadas Baltru�aitis, Peter Robinson, and Louis-Philippe Morency. 
//       in IEEE Int. Conference on Computer Vision Workshops, 300 Faces in-the-Wild Challenge, 2013.    
//
///////////////////////////////////////////////////////////////////////////////

#include "RecorderOpenFace.h"

// For sorting
#include <algorithm>

// File manipulation
#include <fstream>
#include <sstream>
#include <iostream>

// Boost includes for file system manipulation
#include <filesystem.hpp>
#include <filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost::filesystem;

using namespace Recorder;

#define WARN_STREAM( stream ) \
std::cout << "Warning: " << stream << std::endl

void CreateDirectory(std::string output_path)
{

	// Creating the right directory structure
	auto p = path(output_path);

	if (!boost::filesystem::exists(p))
	{
		bool success = boost::filesystem::create_directories(p);

		if (!success)
		{
			std::cout << "Failed to create a directory..." << p.string() << std::endl;
		}
	}
}


RecorderOpenFace::RecorderOpenFace(const std::string out_directory, const std::string in_filename, RecorderOpenFaceParameters parameters):video_writer(), params(parameters)
{

	// From the filename, strip out the name without directory and extension
	filename = path(in_filename).replace_extension("").filename().string();
	record_root = out_directory;

	// Construct the directories required for the output
	CreateDirectory(record_root);

	// Create the required individual recorders, CSV, HOG, aligned, video
	csv_filename = (path(record_root) / path(filename).replace_extension(".csv")).string();

	// Consruct HOG recorder here
	if(params.outputHOG())
	{
		std::string hog_filename = (path(record_root) / path(filename).replace_extension(".hog")).string();
		hog_recorder.Open(hog_filename);
	}

	// saving the videos	
	if (params.outputTrackedVideo())
	{
		this->video_filename = (path(record_root) / path(filename).replace_extension(".avi")).string();
	}

	// TODO aligned Prepare image recording

	observation_count = 0;

}

void RecorderOpenFace::SetObservationVisualization(const cv::Mat &vis_track)
{
	if (params.outputTrackedVideo())
	{
		// Initialize the video writer if it has not been opened yet
		if(!video_writer.isOpened())
		{
			std::string video_filename = (path(record_root) / path(filename).replace_extension(".avi")).string();
			std::string output_codec = params.outputCodec();
			try
			{
				video_writer.open(video_filename, CV_FOURCC(output_codec[0], output_codec[1], output_codec[2], output_codec[3]), params.outputFps(), vis_track.size(), true);
			}
			catch (cv::Exception e)
			{
				WARN_STREAM("Could not open VideoWriter, OUTPUT FILE WILL NOT BE WRITTEN. Currently using codec " << output_codec << ", try using an other one (-oc option)");
			}
		}

		vis_to_out = vis_track;

	}

}


void RecorderOpenFace::WriteObservation()
{
	observation_count++;

	// Write out the CSV file (it will always be there, even if not outputting anything more but frame/face numbers)
	
	if(observation_count == 1)
	{
		// As we are writing out the header, work out some things like number of landmarks, names of AUs etc.
		int num_face_landmarks = landmarks_2D.rows / 2;
		int num_eye_landmarks = eye_landmarks.size();
		int num_model_modes = pdm_params_local.rows / 2;

		std::vector<std::string> au_names_class;
		for (auto au : au_occurences)
		{
			au_names_class.push_back(au.first);
		}

		std::sort(au_names_class.begin(), au_names_class.end());

		std::vector<std::string> au_names_reg;
		for (auto au : au_intensities)
		{
			au_names_reg.push_back(au.first);
		}

		std::sort(au_names_reg.begin(), au_names_reg.end());

		csv_recorder.Open(csv_filename, params.output2DLandmarks(), params.output3DLandmarks(), params.outputPDMParams(), params.outputPose(),
			params.outputAUs(), params.outputGaze(), num_face_landmarks, num_model_modes, num_eye_landmarks, au_names_class, au_names_reg);
	}

	this->csv_recorder.WriteLine(observation_count, timestamp, landmark_detection_success, 
		landmark_detection_confidence, landmarks_2D, landmarks_3D, pdm_params_local, pdm_params_global, head_pose,
		gaze_direction0, gaze_direction1, gaze_angle, eye_landmarks, au_intensities, au_occurences);

	if(params.outputHOG())
	{
		this->hog_recorder.Write();
	}

	if(params.outputTrackedVideo())
	{
		if (vis_to_out.empty())
		{
			WARN_STREAM("Output tracked video frame is not set");
		}
		video_writer.write(vis_to_out);
		// Clear the output
		vis_to_out = cv::Mat();
	}
}


void RecorderOpenFace::SetObservationHOG(bool good_frame, const cv::Mat_<double>& hog_descriptor, int num_cols, int num_rows, int num_channels)
{
	this->hog_recorder.SetObservationHOG(good_frame, hog_descriptor, num_cols, num_rows, num_channels);
}

void RecorderOpenFace::SetObservationTimestamp(double timestamp)
{
	this->timestamp = timestamp;
}

void RecorderOpenFace::SetObservationLandmarks(const cv::Mat_<double>& landmarks_2D, const cv::Mat_<double>& landmarks_3D,
	const cv::Vec6d& pdm_params_global, const cv::Mat_<double>& pdm_params_local, double confidence, bool success)
{
	this->landmarks_2D = landmarks_2D;
	this->landmarks_3D = landmarks_3D;
	this->pdm_params_global = pdm_params_global;
	this->pdm_params_local = pdm_params_local;
	this->landmark_detection_confidence = confidence;
	this->landmark_detection_success = success;

}

void RecorderOpenFace::SetObservationPose(const cv::Vec6d& pose)
{
	this->head_pose = pose;
}

void RecorderOpenFace::SetObservationActionUnits(const std::vector<std::pair<std::string, double> >& au_intensities,
	const std::vector<std::pair<std::string, double> >& au_occurences)
{
	this->au_intensities = au_intensities;
	this->au_occurences = au_occurences;
}

void RecorderOpenFace::SetObservationGaze(const cv::Point3f& gaze_direction0, const cv::Point3f& gaze_direction1,
	const cv::Vec2d& gaze_angle, const std::vector<cv::Point2d>& eye_landmarks)
{
	this->gaze_direction0 = gaze_direction0;
	this->gaze_direction1 = gaze_direction1;
	this->gaze_angle = gaze_angle;
	this->eye_landmarks = eye_landmarks;
}

RecorderOpenFace::~RecorderOpenFace()
{
	this->Close();
}


void RecorderOpenFace::Close()
{
	hog_recorder.Close();
	csv_recorder.Close();
	video_writer.release();
}


