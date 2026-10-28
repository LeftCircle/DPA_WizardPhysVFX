#include "image_editing.h"

ImageEditor::~ImageEditor() {}


void ImageEditor::save_edited_image() {
	if (_edited_image){
		_edited_image->oiio_write();
	}
}

void ImageEditor::save_edited_image(const std::string& file_extension){
	if (_edited_image){
		_edited_image->oiio_write(file_extension);
	}
}

ImageData ImageEditor::optical_flow(
	const std::vector<std::string>& image_sequence,
	const ImageData& img_to_flow,
	std::string output_dir,
	int iterations_per_image
){
	
	std::vector<ImageData> flow_sequence;
	flow_sequence.reserve(image_sequence.size());
	for (const auto& filename : image_sequence) {
		ImageData img(filename.c_str());
		flow_sequence.push_back(img);
	}
	std::vector<size_t> indices;
	for (size_t i = 0; i < flow_sequence.size(); i++) {
		indices.push_back(i);
	}
	OpticalFlow flow_driver(flow_sequence, img_to_flow);
	return flow_driver.flow(indices, output_dir, iterations_per_image);
}

void ImageEditor::optical_flow_video(
	const std::vector<std::string>& video_frame_sequence,
	int flow_frames_per_image,
	std::string output_dir,
	int iterations_per_image,
	bool reverse_sequence,
	bool reverse_flow_direction
){
	
	const ImageData first_img(video_frame_sequence[0].c_str());
	std::vector<ImageData> video_imgs;
	video_imgs.reserve(video_frame_sequence.size());
	for (const auto& filename : video_frame_sequence) {
		ImageData img(filename.c_str());
		video_imgs.push_back(img);
	}

	OpticalFlow flow_driver(video_imgs, first_img);
	
	// Actually performing the flow
	if (reverse_sequence){
		for (size_t i = 0; i < video_frame_sequence.size(); i++){
			flow_driver.set_new_target_image(video_imgs[i]);
			
			std::vector<size_t> flow_sequence;
			size_t flow_frames = std::min(static_cast<size_t>(flow_frames_per_image), i);
			for (int f = 0; f < flow_frames; f++){
				// Try to get the reverse flow. So flow this based on past frames
				flow_sequence.push_back(i - f - 1);
			}
			if (flow_sequence.empty()){
				continue;
			}
			ImageData flowed_image = flow_driver.flow(flow_sequence, "", iterations_per_image);
				
			std::string img_n = StringFuncs::get_zero_padded_number_string(i, 4);
			std::string out_filename = output_dir + "/flowed_" + img_n + "." + flowed_image.get_ext();
			//ImageData blended_image = video_imgs[i].duplicate();
			//blend_images(blended_image, flowed_image, 0.8f);
			// Blend the orignal image with the flowed image
			//blended_image.oiio_write_to(out_filename);
			flowed_image.oiio_write_to(out_filename);
			
			std::cout << "Completed optical flow for frame " << i << " of " << video_frame_sequence.size() << std::endl;
		} // End loop over video frames
	} // End if reverse sequence
	else {
		for (size_t i = 0; i < video_frame_sequence.size() - 1; i++){
			flow_driver.set_new_target_image(video_imgs[i]);
			
			std::vector<size_t> flow_sequence;
			size_t flow_frames = std::min(static_cast<size_t>(flow_frames_per_image), video_frame_sequence.size() - i - 1);
			for (int f = 0; f < flow_frames; f++){
				flow_sequence.push_back(i + f + 1);
			}
			ImageData flowed_image = flow_driver.flow(flow_sequence, "", iterations_per_image);
			std::string img_n = StringFuncs::get_zero_padded_number_string(i, 4);
			std::string out_filename = output_dir + "/flowed_" + img_n + "." + flowed_image.get_ext();
			
			// ImageData blended_image = video_imgs[i].duplicate();
			// blend_images(blended_image, flowed_image, 0.8f);
			// blended_image.oiio_write_to(out_filename);
			flowed_image.oiio_write_to(out_filename);
			std::cout << "Completed optical flow for frame " << i << " of " << video_frame_sequence.size() << std::endl;
		}
	}
}

void ImageEditor::extend_video_duration_to(
	const std::vector<std::string>& video_frame_sequence,
	int fps,
	float target_duration_seconds,
	std::string output_dir,
	int iterations_per_image
){
	
	int current_duration_seconds = static_cast<int>((float)video_frame_sequence.size()) / (float)fps;
	if (target_duration_seconds <= current_duration_seconds){
		std::cout << "Target duration is less than or equal to current duration. No extension performed." << std::endl;
		return;
	}
	// Create the initial images
	std::vector<ImageData> video_imgs;
	video_imgs.reserve(video_frame_sequence.size());
	for (const auto& filename : video_frame_sequence) {
		ImageData img(filename.c_str());
		video_imgs.push_back(img);
	}

	OpticalFlow flow_driver(video_imgs, video_imgs[0]);
	std::string ext = video_imgs[0].get_ext();

	int n_og_images = video_frame_sequence.size();
	int total_frames_needed = target_duration_seconds * fps;
	float uninterp_fps = (float)video_frame_sequence.size() / (float)target_duration_seconds;
	float t = 0.0f;
	float dt = 1.0f / (float)fps;
	float dt_uninterp = 1.0f / uninterp_fps;
	int image_index = 0;
	while (t < target_duration_seconds){
		int start_index = (int)((t / (float)target_duration_seconds) * (n_og_images - 1));
		int end_index = std::min(start_index + 1, static_cast<int>(n_og_images - 1));
		std::cout << "Extending frame " << image_index << ": between original frames " << start_index << " and " << end_index << std::endl;
		float og_start_time = (float)start_index * dt_uninterp;
		float og_end_time = (float)end_index * dt_uninterp;
		float diff = og_end_time - og_start_time;
		float lerp_t = (t - og_start_time) / diff;

		std::cout << "og_start_time: " << og_start_time << ", og_end_time: " << og_end_time << ", diff: " << diff << ", lerp_t: " << lerp_t << std::endl;
		std::cout << "t: " << t << ", dt: " << dt << ", dt_uninterp: " << dt_uninterp << std::endl;
		if (lerp_t > 1.0 || lerp_t < 0.0){
			//throw std::runtime_error("Error in computing lerp_t for video extension.");
			//break;
			start_index += 1;
			end_index = std::min(start_index + 1, static_cast<int>(n_og_images - 1));
			og_start_time = (float)start_index * dt_uninterp;
			og_end_time = (float)end_index * dt_uninterp;
			diff = og_end_time - og_start_time;
			lerp_t = (t - og_start_time) / diff;
			std::cout << "Adjusted lerp_t: " << lerp_t << std::endl;
		}

		// Now we have to flow from the start index to the end index by lerp_t
		flow_driver.set_new_target_image(video_imgs[start_index]);
		std::vector<size_t> flow_sequence = {(size_t)start_index, (size_t)end_index};
		ImageData flowed_image = flow_driver.flow(flow_sequence, "", iterations_per_image, lerp_t);
		std::string img_n = StringFuncs::get_zero_padded_number_string(image_index, 4);
		std::string out_filename = output_dir + "/extended_" + img_n + "." + ext;
		flowed_image.oiio_write_to(out_filename);
		std::cout << "Completed extended frame " << image_index << " of " << total_frames_needed << std::endl;

		image_index++;
		t += dt;
	}
}