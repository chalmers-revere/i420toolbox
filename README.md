Moved to https://git.opendlv.org.

## Microservice with tools to transform an input image in I420 format to two images in I420 and ARGB format

This repository provides source code to transform an input image in I420 format
residing in shared memory to two images in I420 and ARGB format residing two new
shared memory areas suitable for subsequent video processing like compression.

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Table of Contents
* [Dependencies](#dependencies)
* [Usage](#usage)
* [Build from sources on the example of Ubuntu 16.04 LTS](#build-from-sources-on-the-example-of-ubuntu-1604-lts)
* [License](#license)


## Dependencies
You need a C++14-compliant compiler to compile this project.

The following dependency is part of the source distribution:
* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)

The following dependency is downloaded and installed during the Docker-ized build:
* [libyuv](https://chromium.googlesource.com/libyuv/libyuv/+/master) - [![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause) - [Google Patent License Conditions](https://chromium.googlesource.com/libyuv/libyuv/+/master/PATENTS)


## Usage
To run this microservice using `docker-compose`, you can simply add the following
section to your `docker-compose.yml`:

```yml
version: '2' # Must be present exactly once at the beginning of the docker-compose.yml file
services:    # Must be present exactly once at the beginning of the docker-compose.yml file
    i420toolbox:
        image: chalmersrevere/i420toolbox-multi:v0.0.2
        restart: on-failure
        ipc: "host"
        volumes:
        - /tmp:/tmp
        environment:
        - DISPLAY=${DISPLAY}
        command: "--in=video0.i420 --in.width=640 --in.height=480 --flip --out=imgout.i420"
```

Command for commandline to display the resulting image after operations:
```
docker run --rm -ti --init --ipc=host -v /tmp:/tmp -e DISPLAY=$DISPLAY i420toolbox:v0.0.2 --in=video0.i420 --out=image.i420 --in.width=640 --in.height=480 --crop.x=160 --crop.y=120 --crop.width=320 --crop.height=240 --flip --scale.width=640 --scale.height=480 --verbose
```

As this microservice is connecting to an existing shared memory to read the I420
image to transform it into two new shared memory areas using SysV IPC, the `docker-compose.yml`
file specifies the use of `ipc:host`. The folder `/tmp` is shared into the Docker
container to provide tokens describing the shared memory areas.
The parameters to the application are:

* `--in`: Name of the shared memory area containing the I420 image
* `--out`: Name of the shared memory area to be created for the I420 image
* `--out`: Name of the shared memory area to be created for the ARGB image
* `--in.width`: Width of the input image
* `--in.height`: Height of the input image
* `--flip`: Rotate the input image by 180 degrees
* `--crop.x`: Crop this area from the input image (x for top left)
* `--crop.y`: Crop this area from the input image (y for top left)
* `--crop.width`: Crop this area from the input image (width)
* `--crop.height`: Crop this area from the input image (height)
* `--scale.width`: Scale the result from flipping/cropping (width)
* `--scale.height`: Scale the result from flipping/cropping (height)
* `--verbose`: Display the resulting output image to screen (requires X11; run `xhost +` to allow access to you X11 server)


## Build from sources on the example of Ubuntu 16.04 LTS
To build this software, you need cmake, C++14 or newer, libyuv, libvpx, and make.
Having these preconditions, just run `cmake` and `make` as follows:

```
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release ..
make && make test && make install
```


## License

* This project is released under the terms of the GNU GPLv3 License

