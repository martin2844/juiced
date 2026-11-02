# Shared by the distribution and development shell. The engine dynamically
# decodes audio; GPL/nonfree encoders and post-processing are not required.
{ pkgs }:
pkgs.ffmpeg.override {
  withGPL = false;
  withVersion3 = false;
  withUnfree = false;
}
