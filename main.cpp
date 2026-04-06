#include <iostream>
#include "include/Image.h"
using std::cout;

int main() {
  Image src("./images/cat.png");
  Image tgt("./images/target.png");

  Image *fast = src.remapFast(&tgt);
  fast->saveImage(Image::PNG, "./images/remap_fast.png");
  delete fast;

  Image *opt = src.remapOptimized(&tgt, 16);
  opt->saveImage(Image::PNG, "./images/remap_optimized.png");
  delete opt;

  // Generate 60-frame animation using the fast method
  src.generateFrames(&tgt, 60, "./frames", true);

  return 0;
}