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

  // Remap: reorder src pixels to match target's structure.
  // Both images must be the same dimensions.
  // Only pixel positions change — colors are preserved exactly.
  Image *remapFast(Image *target);                          // O(n log n) luminance sort
  Image *remapOptimized(Image *target, int blockSize = 16); // block match + per-block sort

  // Generate animation frames showing pixels moving from this image to target.
  // Frames are saved as outDir/frame_000.png ... frame_NNN.png.
  // Run: ffmpeg -framerate 30 -i outDir/frame_%03d.png output.gif
  void generateFrames(Image *target, int numFrames = 60, string outDir = "./frames",
                      bool fast = true, int blockSize = 16);
};
#endif
