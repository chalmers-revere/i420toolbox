#!/bin/sh

VERSION=$1

cat <<EOF >/tmp/multi.yml
image: chalmersrevere/i420toolbox-multi:$VERSION
manifests:	
  - image: chalmersrevere/i420toolbox-amd64:$VERSION
    platform:
      architecture: amd64
      os: linux
  - image: chalmersrevere/i420toolbox-armhf:$VERSION
    platform:
      architecture: arm
      os: linux
  - image: chalmersrevere/i420toolbox-aarch64:$VERSION
    platform:
      architecture: arm64
      os: linux
EOF
manifest-tool-linux-amd64 push from-spec /tmp/multi.yml
