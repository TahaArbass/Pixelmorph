#ifndef IMAGE_H
#define IMAGE_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
using std::string;

class Image {
  enum AllocationType { NO_ALLOCATION, SELF_ALLOCATED, STB_ALLOCATED };

public:
  enum ImageExtension { JPG, PNG };

private:
  int width;
  int height;
  int channels;
  size_t size;
  uint8_t *data;
  string fname;
  enum AllocationType allocationType;

  Image(uint8_t *data, int width, int height, int channels);

  std::vector<int> permFast(Image *target);
  std::vector<int> permOptimized(Image *target, int blockSize);
  static Image *applyPerm(Image *src, const std::vector<int> &perm);

public:
  Image(string fname);
  ~Image();
  Image *toGray();
  Image *toSepia();
  void saveImage(ImageExtension ext, string outFname = "");

  Image *resizeTo(int newWidth, int newHeight);
};
#endif
