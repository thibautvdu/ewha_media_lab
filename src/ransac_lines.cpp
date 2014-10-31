#include "ransac_lines.h"

namespace garment_augmentation {
namespace math {

using mrpt::math::TPoint3D;
using mrpt::math::TLine3D;
using mrpt::math::CMatrixTemplateNumeric;

	void Ransac3DsegmentFit(const CMatrixDouble  &all_data, const vector_size_t  &use_indices, std::vector< CMatrixDouble > &fit_models) {
		ASSERT_(use_indices.size() == 2);

		try
		{
			fit_models.resize(1);
			CMatrixDouble &M = fit_models[0];

			M.setSize(2, 1);
			M(0, 0) = use_indices[0];
			M(1, 0) = use_indices[1];
		}
		catch (std::exception &)
		{
			fit_models.clear();
			return;
		}
	}

	namespace {
		int k_min_inliers_for_valid_line;
	}
	void Ransac3DsegmentDistance(const CMatrixDouble &all_data, const std::vector< CMatrixDouble > & test_models, const double distance_threshold,
		unsigned int & out_bestModelIndex, vector_size_t & out_inlierIndices) {
		ASSERT_(test_models.size() == 1)
			out_bestModelIndex = 0;
		const CMatrixDouble &M = test_models[0];

		ASSERT_(size(M, 1) == 2 && size(M, 2) == 1);
		TLine3D line;
		try
		{
			line = TLine3D(
				TPoint3D(all_data.get_unsafe(0, M(0, 0)),all_data.get_unsafe(1, M(0, 0)),all_data.get_unsafe(2, M(0, 0))),
				TPoint3D(all_data.get_unsafe(0, M(1, 0)),all_data.get_unsafe(1, M(1, 0)),all_data.get_unsafe(2, M(1, 0)))
			);
		}
		catch (std::logic_error &)
		{
			out_inlierIndices.clear();
			return;
		}

		// Test the verticality of the line first
		double director[3];
		line.getUnitaryDirectorVector(director);
		double angle_cos = /*director[0] * 0 +*/ director[1] * 1 /* + director[2] * 0*/; // angle with vector (0,1,0)
		if (angle_cos < 0.9 && angle_cos > -0.9)
			return;

		const size_t N = size(all_data, 2);
		std::vector<std::pair<size_t, float>> inliers;
		inliers.reserve(30);
		TPoint3D candidate;
		for (size_t i = 0; i<N; i++)
		{
			candidate = TPoint3D(all_data.get_unsafe(0, i), all_data.get_unsafe(1, i), all_data.get_unsafe(2, i));
			const double d = line.distance(candidate);
			if (d < distance_threshold) {
				inliers.push_back(std::pair<size_t, float>(i, candidate.y));
			}
		}

		// Sort the inliers according to the Y axis
		if (inliers.size() > k_min_inliers_for_valid_line) {
			std::sort(inliers.begin(), inliers.end(), ComparePoints);

			// Look for the biggest group if there is undesired gap along Y
			int max_group_begin = 0, max_group_size = 1;
			int temp_group_begin = 0;
			for (int i = 1; i < inliers.size(); ++i) {
				if (inliers[i].second - inliers[i - 1].second > 0.1) { // 10cm
					if ((i - 1) - temp_group_begin > max_group_size) {
						max_group_begin = temp_group_begin;
						max_group_size = (i - 1) - temp_group_begin;
					}
					temp_group_begin = i;
				}
			}
			if ((inliers.size() - 1) - temp_group_begin > max_group_size) {
				max_group_begin = temp_group_begin;
				max_group_size = (inliers.size() - 1) - temp_group_begin;
			}

			out_inlierIndices.clear();
			out_inlierIndices.reserve(max_group_size);
			for (int i = max_group_begin; i < max_group_begin + max_group_size; ++i) {
				out_inlierIndices.push_back(inliers[i].first);
			}
		}
	}

	void RansacDetect3Dsegments(const std::vector<ofVec3f> &point_cloud, const double threshold, const size_t min_inliers_for_valid_line,
		std::vector<std::pair<size_t, Of3dsegment> > &out_detected_segments) {

		out_detected_segments.clear();

		if (point_cloud.empty())
			return;

		k_min_inliers_for_valid_line = min_inliers_for_valid_line;

		// The running lists of remaining points after each plane, as a matrix:
		CMatrixDouble remainingPoints(3, point_cloud.size());
		for (int i = 0; i < point_cloud.size(); ++i) {
			remainingPoints(0, i) = point_cloud[i].x;
			remainingPoints(1, i) = point_cloud[i].y;
			remainingPoints(2, i) = point_cloud[i].z;
		}

		// For each line
		while (size(remainingPoints, 2) >= 2)
		{
			vector_size_t this_best_inliers;
			CMatrixDouble this_best_model;

			mrpt::math::RANSAC::execute(
				remainingPoints,
				Ransac3DsegmentFit,
				Ransac3DsegmentDistance,
				Ransac3DsegmentDegenerate,
				threshold,
				2,  // Minimum set of points
				this_best_inliers,
				this_best_model,
				false, // Verbose
				0.99999  // Prob. of good result
			);

			// Is this line good enough?
			if (this_best_inliers.size() >= min_inliers_for_valid_line)
			{
				// Fit a line to the inliers
				Eigen::MatrixX3d matrix_a(this_best_inliers.size(), 3);
				for (int i = 0; i < this_best_inliers.size(); ++i) {
					int inlier_idx = this_best_inliers[i];
					matrix_a(i, 0) = remainingPoints(0, inlier_idx);
					matrix_a(i, 1) = remainingPoints(1, inlier_idx);
					matrix_a(i, 2) = remainingPoints(2, inlier_idx);
				}

				Eigen::MatrixX3d centered = matrix_a.rowwise() - matrix_a.colwise().mean();
				Eigen::Matrix3d cov = (centered.transpose() * centered) / double(matrix_a.rows());

				Eigen::EigenSolver<Eigen::MatrixXd> solver;
				solver.compute(cov, true);

				size_t max_index;
				double sqrt_eigen_value = std::sqrt(solver.eigenvalues().real().maximum(&max_index));
				Eigen::Vector3d director = solver.eigenvectors().col(max_index).real();
				director.normalize();

				// Get the projection of each inlier on the line
				Eigen::VectorXd a_projection = centered * director;
				std::sort(a_projection.data(), a_projection.data() + a_projection.size());

				double a_max = a_projection[a_projection.size() - 1];
				double a_min = a_projection[0];

				Eigen::Vector3d mean = matrix_a.colwise().mean().transpose();
				Eigen::Vector3d a = mean + a_max * director;
				Eigen::Vector3d b = mean + a_min * director;
				
				/*
				Eigen::Vector3d mean = matrix_a.colwise().mean().transpose();
				Eigen::Vector3d a = mean - sqrt_eigen_value * director;
				Eigen::Vector3d b = mean + sqrt_eigen_value * director;
				*/

				// Discriminate the points on their y coordinate to get consistant results over time
				if (a.y() < b.y())
					out_detected_segments.push_back(std::make_pair<size_t, Of3dsegment>(this_best_inliers.size(), Of3dsegment(b.x(), b.y(), b.z(), a.x(), a.y(), a.z())));
				else
					out_detected_segments.push_back(std::make_pair<size_t, Of3dsegment>(this_best_inliers.size(), Of3dsegment(a.x(), a.y(), a.z(), b.x(), b.y(), b.z())));

				/*
				out_detected_segments.push_back(std::make_pair<size_t, OfEigen3dsegment>(this_best_inliers.size(), 
					OfEigen3dsegment(this_best_model(0, 0), this_best_model(0, 1), this_best_model(0, 2), 
									this_best_model(1, 0), this_best_model(1, 1), this_best_model(1, 2)))); */

				// Discard the selected points so they are not used again for finding subsequent planes:
				remainingPoints.removeColumns(this_best_inliers);
			}
			else
			{
				break; // Do not search for more lines
			}
		}
	}

} // namespace math
} // namespace garment_augmentation