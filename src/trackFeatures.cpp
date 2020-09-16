/****************************************************************************
 *
 *   Copyright (c) 2016 AIT, ETH Zurich. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name AIT nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
/*
 * trackFeatures.cpp
 *
 *  Created on: Mar 9, 2016
 *      Author: nicolas
 */


#include "trackFeatures.h"

bool compare_keypoints(const cv::KeyPoint &first, const cv::KeyPoint &second) {
    return first.response > second.response;
}

FeatureTracker::FeatureTracker( std::vector<int> &feature_status, int number_of_features ){

    if (feature_status.empty()) {
		feature_status.resize(number_of_features, 2); // Request = 2 features = 30 
	}

    this->prev_status.resize(number_of_features, 0);

}

// corners (features, z_all_r) and status are output variables
void FeatureTracker::track_features(const cv::Mat &img, std::vector<cv::Point2f> &features, std::vector<int> &feature_status ) {
    if (!img.data)
        throw "Left image is invalid";

    std::vector<unsigned char> status; // Vector to hold status of each tracked feature from calcOpticalFlowPyrLK
    std::vector<cv::Point2f> corners;  // Vector to hold new corner positions tracked by calcOpticalFlowPyrLK
    std::vector<float> errors;         // not used

    unsigned int numPoints = feature_status.size();

    // Resize features array to size of status, and fill with unvalid points (outside frame)
    features.resize(numPoints);
    std::fill(features.begin(), features.end(), cv::Point2f(-100, -100));


    // NEED COMMENT
    for (size_t i = 0; i < feature_status.size() && i < numPoints; ++i) {
        if (feature_status[i] == 1) {
            prev_status[i] = 1;
        } else {
            prev_status[i] = 0;  // if feature_status[i] == 0 feature is inactive, == 2 request new feature
        }
    }

    if (!prev_img.empty()) {
        if (!prev_corners.empty()) {

            cv::calcOpticalFlowPyrLK(prev_img, img, prev_corners, corners, status, errors, cv::Size(9, 9), 3);
            prev_corners = corners;

            for (size_t i = 0; i < prev_corners.size() && i < numPoints; ++i) {
                // Check if calcOpticalFlowPyrLK lost track of a feature.
                if (!(prev_status[i] && status[i]))
                    prev_status[i] = 0;

                // If last corner was a active feature, check if it is still valid (within the frame)
                if (prev_status[i] == 1) {
                    if (prev_corners[i].x < 0 || prev_corners[i].x > img.cols || prev_corners[i].y < 0 || prev_corners[i].y > img.rows ) {
                        feature_status[i] = 0;
                    } else {
                        // Corner is still a feature, save it by setting feature_status[i] == 1 (active feature)
                        features[i] = prev_corners[i];
                        feature_status[i] = 1;
                    }
                } else {
                    if (feature_status[i] == 1)  // be careful not to overwrite 2s in feature_status
                        feature_status[i] = 0;
                }
            }
        }
    }

    img.copyTo(prev_img);
    
    // initialize new points if needed
    init_more_points(img, features, feature_status );

}

void FeatureTracker::update_feature_status( std::vector<int> &feature_status ){

        //update feature status
	for (int i = 0; i < feature_status.size(); i++) {
		//new and now active
		if (feature_status[i] == 2) {
			feature_status[i] = 1;
		}

		//inactive
		if (feature_status[i] == 0) {
			feature_status[i] = 2;
		}
	}

}

void FeatureTracker::init_more_points(const cv::Mat &img, std::vector<cv::Point2f> &features, std::vector<int> &feature_status ) {
    if (!img.data)
        throw "Image is invalid";

    unsigned int targetNumPoints = 0;
    // count the features that need to be initialized
    for (int i = 0; i < feature_status.size(); i++) {
        if (feature_status[i] == 2)  // 2 means new feature requested
            targetNumPoints++;
    }

    if (!targetNumPoints)
        return;

    std::vector<cv::KeyPoint> keypointsL, goodKeypointsL, unusedKeypoints;
    cv::Mat descriptorsL;

    int numBinsX = 4;
    int numBinsY = 4;
    int binWidth = img.cols / numBinsX;
    int binHeight = img.rows / numBinsY;
    int targetFeaturesPerBin = (feature_status.size() - 1) / (numBinsX * numBinsY) + 1;  // total number of features that should be in each bin

    std::vector<std::vector<int> > featuresPerBin(numBinsX, std::vector<int>(numBinsY, 0));

    // Count the number of active features in each bin
    for (int i = 0; i < prev_corners.size(); i++) {
        if (feature_status[i] == 1) {
            int binX = prev_corners[i].x / binWidth;
            int binY = prev_corners[i].y / binHeight;

            if (binX >= numBinsX) {
                printf("Warning: writing to binX out of bounds: %d >= %d\n", binX, numBinsX);
                continue;
            }
            if (binY >= numBinsY) {
                printf("Warning: writing to binY out of bounds: %d >= %d\n", binY, numBinsY);
                continue;
            }

            featuresPerBin[binX][binY]++;
        }
    }

    unsigned int dist = binWidth / targetFeaturesPerBin;
    // go through each cell and detect features
    for (int x = 0; x < numBinsX; x++) {
        for (int y = 0; y < numBinsY; y++) {
            int neededFeatures = std::max(0, targetFeaturesPerBin - featuresPerBin[x][y]);

            if (neededFeatures) {
                int col_from = x * binWidth;
                int col_to = std::min((x + 1) * binWidth, img.cols);
                int row_from = y * binHeight;
                int row_to = std::min((y + 1) * binHeight, img.rows);

                std::vector<cv::KeyPoint> keypoints, goodKeypointsBin;

                // Detect corners in section using FAST method.
                cv::FAST(img.rowRange(row_from, row_to).colRange(col_from, col_to), keypoints, 10);

                sort(keypoints.begin(), keypoints.end(), compare_keypoints);

                // add bin offsets to the points
                for (int i = 0; i < keypoints.size(); i++) {
                    keypoints[i].pt.x += col_from;
                    keypoints[i].pt.y += row_from;
                }

                // check if the new features are far enough from existing points
                int newPtIdx = 0;
                for (; newPtIdx < keypoints.size(); newPtIdx++) {
                    int new_pt_x = keypoints[newPtIdx].pt.x;
                    int new_pt_y = keypoints[newPtIdx].pt.y;

                    bool far_enough = true;
                    for (int j = 0; j < prev_corners.size(); j++) {
                        if (prev_status[j] == 0)
                            continue;
                        int existing_pt_x = prev_corners[j].x;
                        int existing_pt_y = prev_corners[j].y;
                        if (abs(existing_pt_x - new_pt_x) < dist && abs(existing_pt_y - new_pt_y) < dist) {
                            far_enough = false;
                            unusedKeypoints.push_back(keypoints[newPtIdx]);
                            break;
                        }
                    }
                    if (far_enough) {
                        // check if the new feature is too close to a new one
                        for (int j = 0; j < goodKeypointsBin.size(); j++) {
                            int existing_pt_x = goodKeypointsBin[j].pt.x;
                            int existing_pt_y = goodKeypointsBin[j].pt.y;
                            if (abs(existing_pt_x - new_pt_x) < dist && abs(existing_pt_y - new_pt_y) < dist) {
                                far_enough = false;
                                unusedKeypoints.push_back(keypoints[newPtIdx]);
                                break;
                            }
                        }
                        if (far_enough) {
                            goodKeypointsBin.push_back(keypoints[newPtIdx]);
                            if (goodKeypointsBin.size() == neededFeatures)
                                break;
                        }
                    }
                }
                // insert the good points into the vector containing the new points of the whole image
                goodKeypointsL.insert(goodKeypointsL.end(), goodKeypointsBin.begin(), goodKeypointsBin.end());
                // save the unused keypoints for later
                if (newPtIdx < keypoints.size() - 1) {
                    unusedKeypoints.insert(unusedKeypoints.end(), keypoints.begin() + newPtIdx, keypoints.end());
                }
            }
        }
    }

    // if not many features were requested, we may have found too many features. delete from all bins for equal distancing
    if (goodKeypointsL.size() > targetNumPoints) {
        int numFeaturesToRemove = goodKeypointsL.size() - targetNumPoints;
        int stepSize = targetNumPoints / numFeaturesToRemove + 2;  // make sure the step size is big enough so we dont remove too many features

        std::vector<cv::KeyPoint> goodKeypointsL_shortened;
        for (int i = 0; i < goodKeypointsL.size(); i++) {
            if (i % stepSize) {
                goodKeypointsL_shortened.push_back(goodKeypointsL[i]);
            }
        }
        goodKeypointsL = goodKeypointsL_shortened;
    }

    if (goodKeypointsL.size() < targetNumPoints) {
        // try to insert new points that were not used in the bins
        sort(unusedKeypoints.begin(), unusedKeypoints.end(), compare_keypoints);

        dist /= 2;  // reduce the distance criterion

        for (int newPtIdx = 0; newPtIdx < unusedKeypoints.size(); newPtIdx++) {
            int new_pt_x = unusedKeypoints[newPtIdx].pt.x;
            int new_pt_y = unusedKeypoints[newPtIdx].pt.y;

            bool far_enough = true;
            for (int j = 0; j < prev_corners.size(); j++) {
                if (prev_status[j] == 0)
                    continue;
                int existing_pt_x = prev_corners[j].x;
                int existing_pt_y = prev_corners[j].y;
                if (abs(existing_pt_x - new_pt_x) < dist && abs(existing_pt_y - new_pt_y) < dist) {
                    far_enough = false;
                    break;
                }
            }
            if (far_enough) {
                // check if the new feature is too close to a new one
                for (int j = 0; j < goodKeypointsL.size(); j++) {
                    int existing_pt_x = goodKeypointsL[j].pt.x;
                    int existing_pt_y = goodKeypointsL[j].pt.y;
                    if (abs(existing_pt_x - new_pt_x) < dist && abs(existing_pt_y - new_pt_y) < dist) {
                        far_enough = false;
                        break;
                    }
                }
                if (far_enough) {
                    goodKeypointsL.push_back(unusedKeypoints[newPtIdx]);
                    if (goodKeypointsL.size() == targetNumPoints)
                        break;
                }
            }
        }
    }

    if (goodKeypointsL.empty()) {
        for (int i = 0; i < feature_status.size(); i++) {
            if (feature_status[i] == 2)
                feature_status[i] = 0;
        }
        return;
    }

    std::vector<cv::Point2f> leftPoints;
   
    leftPoints.resize(goodKeypointsL.size());
    for (int i = 0; i < goodKeypointsL.size(); i++)
    {
        leftPoints[i] = goodKeypointsL[i].pt;
    }

    if (leftPoints.size() < targetNumPoints) {
        debug_msg_count ++;
        if (debug_msg_count % 50 == 0) {
            printf("Number of good matches: %d, desired: %d\n", (int) leftPoints.size(), targetNumPoints);
        }
    }

    if (prev_corners.size() < feature_status.size())
        prev_corners.resize(feature_status.size());

    int matches_idx = 0;
    for (int i = 0; i < feature_status.size(); i++) {
        if (feature_status[i] == 2) {
            if (matches_idx < leftPoints.size()) {
                prev_corners[i] = leftPoints[matches_idx];
                prev_status[i] = 1;

                features[i] = leftPoints[matches_idx];

                matches_idx++;
            } else {
                feature_status[i] = 0;
            }
        }
    }
}