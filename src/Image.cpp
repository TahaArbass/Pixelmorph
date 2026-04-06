#include "../include/Image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image/stb_image_write.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <sys/stat.h>
#include <stdexcept>
#include <vector>

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
    *pg     = (uint8_t)fmin(0.393 * *p + 0.769 * *(p + 1) + 0.189 * *(p + 2), 255.0); // R
    *(pg+1) = (uint8_t)fmin(0.349 * *p + 0.686 * *(p + 1) + 0.168 * *(p + 2), 255.0); // G
    *(pg+2) = (uint8_t)fmin(0.272 * *p + 0.534 * *(p + 1) + 0.131 * *(p + 2), 255.0); // B
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

// Returns perm where perm[srcPixelIdx] = tgtPixelIdx
std::vector<int> Image::permFast(Image *target) {
  int n = width * height;
  std::vector<PixelEntry> srcPx(n), tgtPx(n);
  for (int i = 0; i < n; i++) {
    srcPx[i] = {luminance(data + i * channels), i};
    tgtPx[i] = {luminance(target->data + i * target->channels), i};
  }
  std::sort(srcPx.begin(), srcPx.end(), cmpLum);
  std::sort(tgtPx.begin(), tgtPx.end(), cmpLum);

  std::vector<int> perm(n);
  for (int i = 0; i < n; i++)
    perm[srcPx[i].idx] = tgtPx[i].idx;
  return perm;
}

std::vector<int> Image::permOptimized(Image *target, int blockSize) {
  int blocksX = (width  + blockSize - 1) / blockSize;
  int blocksY = (height + blockSize - 1) / blockSize;
  int numBlocks = blocksX * blocksY;

  struct BlockAvg { float r, g, b; };
  std::vector<BlockAvg> srcAvg(numBlocks, {0,0,0});
  std::vector<BlockAvg> tgtAvg(numBlocks, {0,0,0});
  std::vector<int>      blockCount(numBlocks, 0);

  for (int by = 0; by < blocksY; by++) {
    for (int bx = 0; bx < blocksX; bx++) {
      int bidx = by * blocksX + bx;
      int xEnd = std::min((bx + 1) * blockSize, width);
      int yEnd = std::min((by + 1) * blockSize, height);
      for (int y = by * blockSize; y < yEnd; y++) {
        for (int x = bx * blockSize; x < xEnd; x++) {
          uint8_t *sp = data         + (y * width + x) * channels;
          uint8_t *tp = target->data + (y * width + x) * target->channels;
          srcAvg[bidx].r += sp[0]; srcAvg[bidx].g += sp[1]; srcAvg[bidx].b += sp[2];
          tgtAvg[bidx].r += tp[0]; tgtAvg[bidx].g += tp[1]; tgtAvg[bidx].b += tp[2];
          blockCount[bidx]++;
        }
      }
      float cnt = (float)blockCount[bidx];
      srcAvg[bidx].r /= cnt; srcAvg[bidx].g /= cnt; srcAvg[bidx].b /= cnt;
      tgtAvg[bidx].r /= cnt; tgtAvg[bidx].g /= cnt; tgtAvg[bidx].b /= cnt;
    }
  }

  std::vector<int>  blockMap(numBlocks, -1);
  std::vector<bool> tgtUsed(numBlocks, false);
  for (int i = 0; i < numBlocks; i++) {
    float bestDist = FLT_MAX; int bestJ = -1;
    for (int j = 0; j < numBlocks; j++) {
      if (tgtUsed[j]) continue;
      float dr = srcAvg[i].r - tgtAvg[j].r;
      float dg = srcAvg[i].g - tgtAvg[j].g;
      float db = srcAvg[i].b - tgtAvg[j].b;
      float d  = dr*dr + dg*dg + db*db;
      if (d < bestDist) { bestDist = d; bestJ = j; }
    }
    blockMap[i] = bestJ;
    tgtUsed[bestJ] = true;
  }

  int n = width * height;
  std::vector<int> perm(n);
  for (int i = 0; i < numBlocks; i++) {
    int j   = blockMap[i];
    int sby = (i / blocksX) * blockSize, sbx = (i % blocksX) * blockSize;
    int tby = (j / blocksX) * blockSize, tbx = (j % blocksX) * blockSize;
    int sxEnd = std::min(sbx + blockSize, width),  syEnd = std::min(sby + blockSize, height);
    int txEnd = std::min(tbx + blockSize, width),  tyEnd = std::min(tby + blockSize, height);

    std::vector<PixelEntry> sp, tp;
    sp.reserve(blockSize * blockSize);
    tp.reserve(blockSize * blockSize);
    for (int y = sby; y < syEnd; y++)
      for (int x = sbx; x < sxEnd; x++)
        sp.push_back({luminance(data + (y * width + x) * channels), y * width + x});
    for (int y = tby; y < tyEnd; y++)
      for (int x = tbx; x < txEnd; x++)
        tp.push_back({luminance(target->data + (y * width + x) * target->channels), y * width + x});

    std::sort(sp.begin(), sp.end(), cmpLum);
    std::sort(tp.begin(), tp.end(), cmpLum);

    int count = (int)std::min(sp.size(), tp.size());
    for (int k = 0; k < count; k++)
      perm[sp[k].idx] = tp[k].idx;
  }
  return perm;
}

// Apply a permutation: place src pixel i at tgt position perm[i]
Image *Image::applyPerm(Image *src, const std::vector<int> &perm) {
  int n = src->width * src->height;
  uint8_t *out = new uint8_t[src->size];
  for (int i = 0; i < n; i++)
    memcpy(out + perm[i] * src->channels, src->data + i * src->channels, src->channels);
  return new Image(out, src->width, src->height, src->channels);
}

Image *Image::remapFast(Image *target) {
  if (width != target->width || height != target->height)
    throw std::runtime_error("remapFast: images must be the same dimensions");
  return applyPerm(this, permFast(target));
}

Image *Image::remapOptimized(Image *target, int blockSize) {
  if (width != target->width || height != target->height)
    throw std::runtime_error("remapOptimized: images must be the same dimensions");
  return applyPerm(this, permOptimized(target, blockSize));
}

// Smoothstep easing: slow start, fast middle, slow end
static inline float ease(float t) { return t * t * (3.0f - 2.0f * t); }

void Image::generateFrames(Image *target, int numFrames, string outDir,
                            bool fast, int blockSize) {
  if (width != target->width || height != target->height)
    throw std::runtime_error("generateFrames: images must be the same dimensions");

  mkdir(outDir.c_str(), 0755);

  std::vector<int> perm = fast ? permFast(target) : permOptimized(target, blockSize);

  int n = width * height;
  // Precompute src and tgt (x,y) for each pixel
  std::vector<int> srcX(n), srcY(n), tgtX(n), tgtY(n);
  for (int i = 0; i < n; i++) {
    srcX[i] = i % width;         srcY[i] = i / width;
    tgtX[i] = perm[i] % width;   tgtY[i] = perm[i] / width;
  }

  uint8_t *frame = new uint8_t[size];
  char fname[256];

  for (int f = 0; f < numFrames; f++) {
    float t = ease((float)f / (float)(numFrames - 1));

    // Start from the src image so gaps are filled with original content
    memcpy(frame, data, size);

    for (int i = 0; i < n; i++) {
      int x = (int)(srcX[i] + t * (tgtX[i] - srcX[i]) + 0.5f);
      int y = (int)(srcY[i] + t * (tgtY[i] - srcY[i]) + 0.5f);
      x = std::max(0, std::min(width  - 1, x));
      y = std::max(0, std::min(height - 1, y));
      memcpy(frame + (y * width + x) * channels, data + i * channels, channels);
    }

    snprintf(fname, sizeof(fname), "%s/frame_%03d.png", outDir.c_str(), f);
    stbi_write_png(fname, width, height, channels, frame, width * channels);
  }

  delete[] frame;
}

void Image::saveImage(ImageExtension ext, string outFname) {
  if (outFname.empty()) {
    string base = fname.empty() ? "output" : fname.substr(0, fname.find_last_of('.'));
    outFname = base + (ext == PNG ? ".png" : ".jpg");
  }
  if (ext == PNG) {
    stbi_write_png(outFname.c_str(), width, height, channels, data, width * channels);
  } else {
    stbi_write_jpg(outFname.c_str(), width, height, channels, data, 90);
  }
}
