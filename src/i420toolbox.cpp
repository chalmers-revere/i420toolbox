/*
 * Copyright (C) 2019  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"

#include <libyuv.h>
#include <libyuv/video_common.h>
#include <X11/Xlib.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    auto cropCounter{
        commandlineArguments.count("crop.x") +
        commandlineArguments.count("crop.y") +
        commandlineArguments.count("crop.width") +
        commandlineArguments.count("crop.height")
    };
    auto scaleCounter{
        commandlineArguments.count("scale.width") +
        commandlineArguments.count("scale.height")
    };
    if ( (0 == commandlineArguments.count("in")) ||
         (0 == commandlineArguments.count("in.width")) ||
         (0 == commandlineArguments.count("in.height")) ||
         (0 == commandlineArguments.count("out")) ||
         ( (0 != cropCounter) && (4 != cropCounter) ) ||
         ( (0 != scaleCounter) && (2 != scaleCounter) ) ) {
        std::cerr << argv[0] << " waits on a shared memory containing an image in I420 format to apply image operations resulting into two corresponding images in I420 and ARGB format in two other shared memory areas." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --in=<name of shared memory for the I420 image> --in.width=<width> --in.height=<height> --out=<name of shared memory to be created for the I420 image> [--flip] [--crop.x=<x> --crop.y=<y> --crop.width=<width> --crop.height=<height>] [--scale.width=<width> --scale.height=<height>] [--verbose]" << std::endl;
        std::cerr << "         --in:         name of the shared memory area containing the I420 image" << std::endl;
        std::cerr << "         --out:        name of the shared memory area to be created for the I420 image" << std::endl;
        std::cerr << "         --out.argb:   name of the shared memory area to be created for the ARGB image (default: value from --out + '.argb')" << std::endl;
        std::cerr << "         --in.width:     width of the input image" << std::endl;
        std::cerr << "         --in.height:    height of the input image" << std::endl;
        std::cerr << "         --crop.x:       crop this area from the input image (x for top left)" << std::endl;
        std::cerr << "         --crop.y:       crop this area from the input image (y for top left)" << std::endl;
        std::cerr << "         --crop.width:   crop this area from the input image (width)" << std::endl;
        std::cerr << "         --crop.height:  crop this area from the input image (height)" << std::endl;
        std::cerr << "         --scale.width:  scale optionally cropped area to this final width" << std::endl;
        std::cerr << "         --scale.height: scale optionally cropped area to this final height" << std::endl;
        std::cerr << "         --flip:         rotate image by 180 degrees" << std::endl;
        std::cerr << "         --verbose:      display output image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --in=video0.i420 --in.width=640 --in.height=480 --flip --out=imgout.i420 --verbose" << std::endl;
    }
    else {
        const std::string IN{commandlineArguments["in"]};
        const std::string OUT{commandlineArguments["out"]};
        const std::string OUT_ARGB{(commandlineArguments.count("out.argb") != 0) ? commandlineArguments["out.argb"] : (OUT + ".argb")};
        const std::string OUT_SCALE{(OUT + ".scale")};
        const uint32_t IN_WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["in.width"]))};
        const uint32_t IN_HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["in.height"]))};
        const uint32_t CROP_X{(commandlineArguments.count("crop.x") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["crop.x"])) : 0u};
        const uint32_t CROP_Y{(commandlineArguments.count("crop.y") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["crop.y"])) : 0u};
        const uint32_t CROP_WIDTH{(commandlineArguments.count("crop.width") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["crop.width"])) : IN_WIDTH};
        const uint32_t CROP_HEIGHT{(commandlineArguments.count("crop.height") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["crop.height"])) : IN_HEIGHT};
        const uint32_t OUT_WIDTH{CROP_WIDTH};
        const uint32_t OUT_HEIGHT{CROP_HEIGHT};
        const uint32_t SCALE_WIDTH{(commandlineArguments.count("scale.width") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["scale.width"])) : 0u};
        const uint32_t SCALE_HEIGHT{(commandlineArguments.count("scale.height") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["scale.height"])) : 0u};
        const uint32_t ROTATE{(commandlineArguments.count("flip") != 0) ? 180u : 0u};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};

        const uint32_t TEMP_WIDTH{(0 < SCALE_WIDTH) ? OUT_WIDTH : 0};
        const uint32_t TEMP_HEIGHT{(0 < SCALE_HEIGHT) ? OUT_HEIGHT : 0};
        const uint32_t FINAL_WIDTH{(0 < SCALE_WIDTH) ? SCALE_WIDTH : OUT_WIDTH};
        const uint32_t FINAL_HEIGHT{(0 < SCALE_HEIGHT) ? SCALE_HEIGHT : OUT_HEIGHT};

        std::unique_ptr<cluon::SharedMemory> sharedMemoryIN;
        std::vector<char> inputImageBuffer;
        std::vector<char> tempImageBuffer;
        std::unique_ptr<cluon::SharedMemory> sharedMemoryOUT_I420;
        std::unique_ptr<cluon::SharedMemory> sharedMemoryOUT_ARGB;

        sharedMemoryIN.reset(new cluon::SharedMemory{IN});
        if (sharedMemoryIN && sharedMemoryIN->valid()) {
            std::clog << "[i420toolbox]: Attached to '" << sharedMemoryIN->name() << "' (" << sharedMemoryIN->size() << " bytes)." << std::endl;
            inputImageBuffer.reserve(sharedMemoryIN->size());
        }
        else {
            std::cerr << "[i420toolbox]: Failed to attach to shared memory '" << IN << "'." << std::endl;
            return retCode;
        }

        if ( 0 < (TEMP_WIDTH * TEMP_HEIGHT) ) {
            tempImageBuffer.reserve(TEMP_WIDTH * TEMP_HEIGHT * 3/2);
        } 

        sharedMemoryOUT_I420.reset(new cluon::SharedMemory{OUT, FINAL_WIDTH * FINAL_HEIGHT * 3/2});
        if (sharedMemoryOUT_I420 && sharedMemoryOUT_I420->valid()) {
            std::clog << "[i420toolbox]: Created shared memory " << OUT << " (" << sharedMemoryOUT_I420->size() << " bytes) for an I420 image (width = " << FINAL_WIDTH << ", height = " << FINAL_HEIGHT << ")." << std::endl;
        }
        else {
            std::cerr << "[i420toolbox]: Failed to create shared memory for output image (I420)." << std::endl;
            return retCode;
        }

        sharedMemoryOUT_ARGB.reset(new cluon::SharedMemory{OUT_ARGB, FINAL_WIDTH * FINAL_HEIGHT * 4});
        if (sharedMemoryOUT_ARGB && sharedMemoryOUT_ARGB->valid()) {
            std::clog << "[i420toolbox]: Created shared memory " << OUT_ARGB << " (" << sharedMemoryOUT_ARGB->size() << " bytes) for an ARGB image (width = " << FINAL_WIDTH << ", height = " << FINAL_HEIGHT << ")." << std::endl;
        }
        else {
            std::cerr << "[i420toolbox]: Failed to create shared memory for output image (ARGB)." << std::endl;
            return retCode;
        }

        Display *display{nullptr};
        Visual *visual{nullptr};
        Window window{0};
        XImage *ximage{nullptr};

        if (VERBOSE) {
            display = XOpenDisplay(NULL);
            visual = DefaultVisual(display, 0);
            window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, FINAL_WIDTH, FINAL_HEIGHT, 1, 0, 0);
            ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(sharedMemoryOUT_ARGB->data()), FINAL_WIDTH, FINAL_HEIGHT, 32, 0);
            XMapWindow(display, window);
        }

        cluon::data::TimeStamp sampleTimeStamp;
        while (!cluon::TerminateHandler::instance().isTerminated) {
            sampleTimeStamp = cluon::time::now();

            sharedMemoryIN->wait();
            sharedMemoryIN->lock();
            {
                // Read notification timestamp.
                auto r = sharedMemoryIN->getTimeStamp();
                sampleTimeStamp = (r.first ? r.second : sampleTimeStamp);
                std::memcpy(inputImageBuffer.data(), reinterpret_cast<uint8_t*>(sharedMemoryIN->data()), sharedMemoryIN->size());
            }
            sharedMemoryIN->unlock();

            sharedMemoryOUT_I420->lock();
            sharedMemoryOUT_I420->setTimeStamp(sampleTimeStamp);
            {
                if ( 0 < (TEMP_WIDTH * TEMP_HEIGHT) ) {
                    // If the image shall be scaled, transform the flipping/cropping operation first and then, render the resulting scaled image into the output area.
                    libyuv::ConvertToI420(reinterpret_cast<uint8_t*>(inputImageBuffer.data()), IN_WIDTH * IN_HEIGHT * 3/2 /* 3/2*IN_WIDTH for I420*/,
                                          reinterpret_cast<uint8_t*>(tempImageBuffer.data()), TEMP_WIDTH,
                                          reinterpret_cast<uint8_t*>(tempImageBuffer.data()+(TEMP_WIDTH * TEMP_HEIGHT)), TEMP_WIDTH/2,
                                          reinterpret_cast<uint8_t*>(tempImageBuffer.data()+(TEMP_WIDTH * TEMP_HEIGHT + ((TEMP_WIDTH * TEMP_HEIGHT) >> 2))), TEMP_WIDTH/2,
                                          CROP_X, CROP_Y,
                                          IN_WIDTH, IN_HEIGHT,
                                          CROP_WIDTH, CROP_HEIGHT,
                                          static_cast<libyuv::RotationMode>(ROTATE), FOURCC('I', '4', '2', '0'));

                    libyuv::I420Scale(reinterpret_cast<uint8_t*>(tempImageBuffer.data()), TEMP_WIDTH,
                                      reinterpret_cast<uint8_t*>(tempImageBuffer.data()+(TEMP_WIDTH * TEMP_HEIGHT)), TEMP_WIDTH/2,
                                      reinterpret_cast<uint8_t*>(tempImageBuffer.data()+(TEMP_WIDTH * TEMP_HEIGHT + ((TEMP_WIDTH * TEMP_HEIGHT) >> 2))), TEMP_WIDTH/2,
                                      TEMP_WIDTH, TEMP_HEIGHT,
                                      reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()), FINAL_WIDTH,
                                      reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()+(FINAL_WIDTH * FINAL_HEIGHT)), FINAL_WIDTH/2,
                                      reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()+(FINAL_WIDTH * FINAL_HEIGHT + ((FINAL_WIDTH * FINAL_HEIGHT) >> 2))), FINAL_WIDTH/2,
                                      FINAL_WIDTH, FINAL_HEIGHT,
                                      libyuv::kFilterNone);
                }
                else {
                    libyuv::ConvertToI420(reinterpret_cast<uint8_t*>(inputImageBuffer.data()), IN_WIDTH * IN_HEIGHT * 3/2 /* 3/2*IN_WIDTH for I420*/,
                                          reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()), FINAL_WIDTH,
                                          reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()+(FINAL_WIDTH * FINAL_HEIGHT)), FINAL_WIDTH/2,
                                          reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()+(FINAL_WIDTH * FINAL_HEIGHT + ((FINAL_WIDTH * FINAL_HEIGHT) >> 2))), FINAL_WIDTH/2,
                                          CROP_X, CROP_Y,
                                          IN_WIDTH, IN_HEIGHT,
                                          CROP_WIDTH, CROP_HEIGHT,
                                          static_cast<libyuv::RotationMode>(ROTATE), FOURCC('I', '4', '2', '0'));
                }

                sharedMemoryOUT_ARGB->lock();
                sharedMemoryOUT_ARGB->setTimeStamp(sampleTimeStamp);
                {
                    libyuv::I420ToARGB(reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()), FINAL_WIDTH,
                                       reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()+(FINAL_WIDTH * FINAL_HEIGHT)), FINAL_WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryOUT_I420->data()+(FINAL_WIDTH * FINAL_HEIGHT + ((FINAL_WIDTH * FINAL_HEIGHT) >> 2))), FINAL_WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryOUT_ARGB->data()), FINAL_WIDTH * 4, FINAL_WIDTH, FINAL_HEIGHT);

                    if (VERBOSE) {
                        XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, FINAL_WIDTH, FINAL_HEIGHT);
                    }
                }
                sharedMemoryOUT_ARGB->unlock();
            }
            sharedMemoryOUT_I420->unlock();


            // Notify listeners.
            sharedMemoryOUT_I420->notifyAll();
            sharedMemoryOUT_ARGB->notifyAll();
        }

        if (VERBOSE) {
            XCloseDisplay(display);
        }
        retCode = 0;
    }
    return retCode;
}

