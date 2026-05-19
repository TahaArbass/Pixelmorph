#include "../include/Image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image/stb_image_write.h"
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "../include/stb_image/stb_image_resize2.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>

Image::Image(string fname) : fname(fname), allocationType(STB_ALLOCATED) {
  data = stbi_load(fname.c_str(), &width, &height, &channels, 0);
  if (data == nullptr) {
    throw std::runtime_error("Failed to load image: " + fname);
  }
  size = (size_t)width * height * channels;
}

// Private constructor for creating Image from raw pixel data (SELF_ALLOCATED)
Image::Image(uint8_t *imgData, int w, int h, int ch)
    : width(w), height(h), channels(ch), size((size_t)w * h * ch),
      data(imgData), allocationType(SELF_ALLOCATED) {}

Image::~Image() {
  if (allocationType == STB_ALLOCATED) {
    stbi_image_free(data);
  } else if (allocationType == SELF_ALLOCATED) {
    delete[] data;
  }
}

Image *Image::toGray() {
  int grayChannels = channels == 4 ? 2 : 1;
  size_t grayImgSize = (size_t)height * width * grayChannels;
  uint8_t *grayImg = new uint8_t[grayImgSize];

  for (uint8_t *p = data, *gp = grayImg; p != data + size;
       p += channels, gp += grayChannels) {
    *gp = (uint8_t)((*p + *(p + 1) + *(p + 2)) / 3.0f);
    if (channels == 4) {
      *(gp + 1) = *(p + 3);
    }
  }
  return new Image(grayImg, width, height, grayChannels);
}

Image *Image::toSepia() {
  uint8_t *sepiaImg = new uint8_t[size];

  for (uint8_t *p = data, *pg = sepiaImg; p != data + size;
       p += channels, pg += channels) {
    *pg = (uint8_t)fmin(0.393 * *p + 0.769 * *(p + 1) + 0.189 * *(p + 2),
                        255.0); // R
    *(pg + 1) = (uint8_t)fmin(0.349 * *p + 0.686 * *(p + 1) + 0.168 * *(p + 2),
                              255.0); // G
    *(pg + 2) = (uint8_t)fmin(0.272 * *p + 0.534 * *(p + 1) + 0.131 * *(p + 2),
                              255.0); // B
    if (channels == 4) {
      *(pg + 3) = *(p + 3);
    }
  }
  return new Image(sepiaImg, width, height, channels);
}

// Shared helper: sort a list of (luminance, flat_pixel_index) pairs
struct PixelEntry {
  float lum;
  int idx; // flat pixel index (not byte index)
};

static inline float luminance(const uint8_t *p) {
  return 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
}

static bool cmpLum(const PixelEntry &a, const PixelEntry &b) {
  return a.lum < b.lum;
}

Image *Image::resizeTo(int newWidth, int newHeight) {
  uint8_t *newData = new uint8_t[newWidth * newHeight * channels];

  // Map our channel count to stb's pixel layout
  stbir_pixel_layout layout;
  switch (channels) {
  case 1:
    layout = STBIR_1CHANNEL;
    break; // STBIR_GREY if it exists
  case 2:
    layout = STBIR_2CHANNEL;
    break; // Grey + Alpha
  case 3:
    layout = STBIR_RGB;
    break;
  case 4:
    layout = STBIR_RGBA;
    break;
  default:
    delete[] newData;
    throw std::runtime_error("Unsupported number of channels");
  }

  // For images, you usually want the sRGB variant because most photos
  // are in sRGB color space and it handles gamma correctly
  unsigned char *result = stbir_resize_uint8_srgb(
      data,      // Input pixels
      width,     // Input width
      height,    // Input height
      0,         // Input stride (0 = tightly packed, same as width * channels)
      newData,   // Output pixels
      newWidth,  // Output width
      newHeight, // Output height
      0,         // Output stride (0 = tightly packed)
      layout     // Pixel layout
  );

  if (result == nullptr) {
    delete[] newData;
    throw std::runtime_error("Failed to resize image");
  }

  // stbir_resize_uint8_srgb returns the output buffer pointer on success
  return new Image(newData, newWidth, newHeight, channels);
}

void Image::saveImage(ImageExtension ext, string outFname) {
  if (outFname.empty()) {
    string base =
        fname.empty() ? "output" : fname.substr(0, fname.find_last_of('.'));
    outFname = base + (ext == PNG ? ".png" : ".jpg");
  }
  if (ext == PNG) {
    stbi_write_png(outFname.c_str(), width, height, channels, data,
                   width * channels);
  } else {
    stbi_write_jpg(outFname.c_str(), width, height, channels, data, 90);
  }
}
