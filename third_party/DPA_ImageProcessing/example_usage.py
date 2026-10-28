#!/usr/bin/env python3
"""
Example usage of the image_editor Python bindings.

Before running this script, build the bindings with:
    make python

Then run:
    python example_usage.py
"""

import image_editor

def main():
    # Load an image
    print("Loading image...")
    img = image_editor.ImageData("test_images/walk_colored.exr")
    
    # Get image properties
    print(f"Image dimensions: {img.get_width()} x {img.get_height()}")
    print(f"Channels: {img.get_channels()}")
    
    # Get a pixel value
    pixel = img.get_pixel_values(100, 100)
    print(f"Pixel at (100, 100): {list(pixel)}")
    
    # Use ImageDataModifier static methods
    print("Applying gamma filter...")
    image_editor.ImageDataModifier.gamma_filter(img, 2.2)
    
    # Scale pixel values
    img.scale_values(0.5)
    print("Scaled image values by 0.5")
    
    # Convert to greyscale
    grey_img = image_editor.ImageDataModifier.greyscale(img)
    print(f"Greyscale image: {grey_img.get_width()} x {grey_img.get_height()}")
    
    # Save the image
    # img.oiio_write()
    
    print("Done!")

if __name__ == "__main__":
    main()
