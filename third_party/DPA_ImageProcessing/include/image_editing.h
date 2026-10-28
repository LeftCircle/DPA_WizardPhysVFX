#ifndef IMAGE_EDITING_H
#define IMAGE_EDITING_H

#include <vector>
#include <memory>
#include <string>

#include "image_data.h"
#include "optical_flow.h"
#include "image_data_modifier.h"

class ImageEditor {
public:
	ImageEditor(ImageData& image_data)
		: _starting_image(&image_data), _edited_image(new ImageData(image_data)) {}
	ImageEditor() {};
	~ImageEditor();
	const std::shared_ptr<ImageData> get_starting_image() const { return _starting_image; }
	const std::shared_ptr<ImageData> get_edited_image() const { return _edited_image; }

	void set_edited_image_to(const ImageData& new_image) {_edited_image = std::make_shared<ImageData>(new_image);}

	void save_edited_image();
	void save_edited_image(const std::string& file_extension);

	ImageData optical_flow(
		const std::vector<std::string>& image_sequence,
		const ImageData& img_to_flow,
		std::string output_dir = "", 
		int iterations_per_image = 1
	);

	// Takes in a video, then applies n frames of optical flow to each frame.
	void optical_flow_video(
		const std::vector<std::string>& video_frame_sequence,
		int flow_frames_per_image = 5,
		std::string output_dir = "",
		int iterations_per_image = 1,
		bool reverse_sequence = false,
		bool reverse_flow_direction = false
	);

	void extend_video_duration_to(
		const std::vector<std::string>& video_frame_sequence,
		int fps,
		float target_duration_seconds,
		std::string output_dir = "",
		int iterations_per_image = 1
	);

private:

	std::shared_ptr<ImageData> _starting_image;
	std::shared_ptr<ImageData> _edited_image;

};



#endif // IMAGE_EDITING_H