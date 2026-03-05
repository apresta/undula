# Undula

This C++ program generates smoothly-evolving textures by blending a user-specified set of images. With the right source material, this can result in animations with a "waving" or "breathing" feel, as seen in [this example](https://player.vimeo.com/video/1170851335?autopause=0&loop=1&transparent=0&quality=4k).

<p align="center">
<a href="https://player.vimeo.com/video/1170851335?autopause=0&loop=1&transparent=0&quality=4k">
  <img src="https://i.vimeocdn.com/video/2130143164-c4ca7e9c97f4bf5fbc42242bae4712b11298669aca5e723d2026b789382940c7-d?mw=2000&mh=1127&q=70" width=1024 alt="Video of evolving flowers texture" />
</a>
</p>

The only external dependency is [OpenCV](https://www.opencv.org) for image processing and rendering.

**Usage**:

```sh
# Install OpenCV dependency (on macOS).
brew install opencv

# Build the executable.
mkdir -p build
cmake -S . -B build
cmake --build build -j 8

# Run in real-time mode (assuming an "images" folder).
# Quit by pressing Ctrl-C in the terminal.
build/undula --images_path images/

# Run with a fixed random seed for repeatability.
build/undula --images_path images/ --seed 1234567890

# Change the animation speed by a factor.
build/undula --images_path images/ --speedup 2.5

# Run in offline mode to output a video (10 minutes at 30 frames per second).
# The output video will be named after the random seed.
mkdir -p recordings
build/undula --images_path images/ --recordings_path recordings/ --record_mins 10 --record_fps 30
```
