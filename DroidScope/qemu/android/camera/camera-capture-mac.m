/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Contains code that is used to capture video frames from a camera device
 * on Mac. This code uses QTKit API to work with camera devices, and requires
 * Mac OS at least 10.5
 */

#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>
#import <CoreAudio/CoreAudio.h>
#include "android/camera/camera-capture.h"
#include "android/camera/camera-format-converters.h"

#define  E(...)    derror(__VA_ARGS__)
#define  W(...)    dwarning(__VA_ARGS__)
#define  D(...)    VERBOSE_PRINT(camera,__VA_ARGS__)

/*******************************************************************************
 *                     Helper routines
 ******************************************************************************/

/* Converts internal QT pixel format to a FOURCC value. */
static uint32_t
_QTtoFOURCC(uint32_t qt_pix_format)
{
  switch (qt_pix_format) {
    case kCVPixelFormatType_24RGB:
      return V4L2_PIX_FMT_RGB24;

    case kCVPixelFormatType_24BGR:
      return V4L2_PIX_FMT_BGR32;

    case kCVPixelFormatType_32ARGB:
    case kCVPixelFormatType_32RGBA:
      return V4L2_PIX_FMT_RGB32;

    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32ABGR:
      return V4L2_PIX_FMT_BGR32;

    case kCVPixelFormatType_422YpCbCr8:
      return V4L2_PIX_FMT_UYVY;

    case kCVPixelFormatType_420YpCbCr8Planar:
      return V4L2_PIX_FMT_YVU420;

    case 'yuvs':  // kCVPixelFormatType_422YpCbCr8_yuvs - undeclared?
      return V4L2_PIX_FMT_YUYV;

    default:
      E("Unrecognized pixel format '%.4s'", (const char*)&qt_pix_format);
      return 0;
  }
}

/*******************************************************************************
 *                     MacCamera implementation
 ******************************************************************************/

/* Encapsulates a camera device on MacOS */
@interface MacCamera : NSObject {
    /* Capture session. */
    QTCaptureSession*             capture_session;
    /* Camera capture device. */
    QTCaptureDevice*              capture_device;
    /* Input device registered with the capture session. */
    QTCaptureDeviceInput*         input_device;
    /* Output device registered with the capture session. */
    QTCaptureVideoPreviewOutput*  output_device;
    /* Current framebuffer. */
    CVImageBufferRef              current_frame;
    /* Desired frame width */
    int                           desired_width;
    /* Desired frame height */
    int                           desired_height;
}

/* Initializes MacCamera instance.
 * Return:
 *  Pointer to initialized instance on success, or nil on failure.
 */
- (MacCamera*)init;

/* Undoes 'init' */
- (void)free;

/* Starts capturing video frames.
 * Param:
 *  width, height - Requested dimensions for the captured video frames.
 * Return:
 *  0 on success, or !=0 on failure.
 */
- (int)start_capturing:(int)width:(int)height;

/* Captures a frame from the camera device.
 * Param:
 *  framebuffers - Array of framebuffers where to read the frame. Size of this
 *      array is defined by the 'fbs_num' parameter. Note that the caller must
 *      make sure that buffers are large enough to contain entire frame captured
 *      from the device.
 *  fbs_num - Number of entries in the 'framebuffers' array.
 * Return:
 *  0 on success, or non-zero value on failure. There is a special vaule 1
 *  returned from this routine which indicates that frames are not yet available
 *  in the device. The client should respond to this value by repeating the
 *  read, rather than reporting an error.
 */
- (int)read_frame:(ClientFrameBuffer*)framebuffers:(int)fbs_num:(float)r_scale:(float)g_scale:(float)b_scale:(float)exp_comp;

@end

@implementation MacCamera

- (MacCamera*)init
{
    NSError *error;
    BOOL success;

    /* Obtain the capture device, make sure it's not used by another
     * application, and open it. */
    capture_device =
        [QTCaptureDevice defaultInputDeviceWithMediaType:QTMediaTypeVideo];
    if (capture_device == nil) {
        E("There are no available video devices found.");
        [self release];
        return nil;
    }
    if ([capture_device isInUseByAnotherApplication]) {
        E("Default camera device is in use by another application.");
        [capture_device release];
        capture_device = nil;
        [self release];
        return nil;
    }
    success = [capture_device open:&error];
    if (!success) {
        E("Unable to open camera device: '%s'",
          [[error localizedDescription] UTF8String]);
        [self free];
        [self release];
        return nil;
    }

    /* Create capture session. */
    capture_session = [[QTCaptureSession alloc] init];
    if (capture_session == nil) {
        E("Unable to create capure session.");
        [self free];
        [self release];
        return nil;
    }

    /* Create an input device and register it with the capture session. */
    input_device = [[QTCaptureDeviceInput alloc] initWithDevice:capture_device];
    success = [capture_session addInput:input_device error:&error];
    if (!success) {
        E("Unable to initialize input device: '%s'",
          [[error localizedDescription] UTF8String]);
        [input_device release];
        input_device = nil;
        [self free];
        [self release];
        return nil;
    }

    /* Create an output device and register it with the capture session. */
    output_device = [[QTCaptureVideoPreviewOutput alloc] init];
    success = [capture_session addOutput:output_device error:&error];
    if (!success) {
        E("Unable to initialize output device: '%s'",
          [[error localizedDescription] UTF8String]);
        [output_device release];
        output_device = nil;
        [self free];
        [self release];
        return nil;
    }
    [output_device setDelegate:self];

    return self;
}

- (void)free
{
    /* Uninitialize capture session. */
    if (capture_session != nil) {
        /* Make sure that capturing is stopped. */
        if ([capture_session isRunning]) {
            [capture_session stopRunning];
        }
        /* Detach input and output devices from the session. */
        if (input_device != nil) {
            [capture_session removeInput:input_device];
            [input_device release];
            input_device = nil;
        }
        if (output_device != nil) {
            [capture_session removeOutput:output_device];
            [output_device release];
            output_device = nil;
        }
        /* Destroy capture session. */
        [capture_session release];
        capture_session = nil;
    }

    /* Uninitialize capture device. */
    if (capture_device != nil) {
        /* Make sure device is not opened. */
        if ([capture_device isOpen]) {
            [capture_device close];
        }
        [capture_device release];
        capture_device = nil;
    }

    /* Release current framebuffer. */
    if (current_frame != nil) {
       CVBufferRelease(current_frame);
       current_frame = nil;
    }
}

- (int)start_capturing:(int)width:(int)height
{
  if (![capture_session isRunning]) {
        /* Set desired frame dimensions. */
        desired_width = width;
        desired_height = height;
        [output_device setPixelBufferAttributes:
          [NSDictionary dictionaryWithObjectsAndKeys:
              [NSNumber numberWithInt: width], kCVPixelBufferWidthKey,
              [NSNumber numberWithInt: height], kCVPixelBufferHeightKey,
              nil]];
        [capture_session startRunning];
        return 0;
  } else if (width == desired_width && height == desired_height) {
      W("%s: Already capturing %dx%d frames",
        __FUNCTION__, desired_width, desired_height);
      return -1;
  } else {
      E("%s: Already capturing %dx%d frames. Requested frame dimensions are %dx%d",
        __FUNCTION__, desired_width, desired_height, width, height);
      return -1;
  }
}

- (int)read_frame:(ClientFrameBuffer*)framebuffers:(int)fbs_num:(float)r_scale:(float)g_scale:(float)b_scale:(float)exp_comp
{
    int res = -1;

    /* Frames are pushed by QT in another thread.
     * So we need a protection here. */
    @synchronized (self)
    {
        if (current_frame != nil) {
            /* Collect frame info. */
            const uint32_t pixel_format =
                _QTtoFOURCC(CVPixelBufferGetPixelFormatType(current_frame));
            const int frame_width = CVPixelBufferGetWidth(current_frame);
            const int frame_height = CVPixelBufferGetHeight(current_frame);
            const size_t frame_size =
                CVPixelBufferGetBytesPerRow(current_frame) * frame_height;

            /* Get framebuffer pointer. */
            CVPixelBufferLockBaseAddress(current_frame, 0);
            const void* pixels = CVPixelBufferGetBaseAddress(current_frame);
            if (pixels != nil) {
                /* Convert framebuffer. */
                res = convert_frame(pixels, pixel_format, frame_size,
                                    frame_width, frame_height,
                                    framebuffers, fbs_num,
                                    r_scale, g_scale, b_scale, exp_comp);
            } else {
                E("%s: Unable to obtain framebuffer", __FUNCTION__);
                res = -1;
            }
            CVPixelBufferUnlockBaseAddress(current_frame, 0);
        } else {
            /* First frame didn't come in just yet. Let the caller repeat. */
            res = 1;
        }
    }

    return res;
}

- (void)captureOutput:(QTCaptureOutput*) captureOutput
                      didOutputVideoFrame:(CVImageBufferRef)videoFrame
                      withSampleBuffer:(QTSampleBuffer*) sampleBuffer
                      fromConnection:(QTCaptureConnection*) connection
{
    CVImageBufferRef to_release;
    CVBufferRetain(videoFrame);

    /* Frames are pulled by the client in another thread.
     * So we need a protection here. */
    @synchronized (self)
    {
        to_release = current_frame;
        current_frame = videoFrame;
    }
    CVBufferRelease(to_release);
}

@end

/*******************************************************************************
 *                     CameraDevice routines
 ******************************************************************************/

typedef struct MacCameraDevice MacCameraDevice;
/* MacOS-specific camera device descriptor. */
struct MacCameraDevice {
    /* Common camera device descriptor. */
    CameraDevice  header;
    /* Actual camera device object. */
    MacCamera*    device;
};

/* Allocates an instance of MacCameraDevice structure.
 * Return:
 *  Allocated instance of MacCameraDevice structure. Note that this routine
 *  also sets 'opaque' field in the 'header' structure to point back to the
 *  containing MacCameraDevice instance.
 */
static MacCameraDevice*
_camera_device_alloc(void)
{
    MacCameraDevice* cd = (MacCameraDevice*)malloc(sizeof(MacCameraDevice));
    if (cd != NULL) {
        memset(cd, 0, sizeof(MacCameraDevice));
        cd->header.opaque = cd;
    } else {
        E("%s: Unable to allocate MacCameraDevice instance", __FUNCTION__);
    }
    return cd;
}

/* Uninitializes and frees MacCameraDevice descriptor.
 * Note that upon return from this routine memory allocated for the descriptor
 * will be freed.
 */
static void
_camera_device_free(MacCameraDevice* cd)
{
    if (cd != NULL) {
        if (cd->device != NULL) {
            [cd->device free];
            [cd->device release];
            cd->device = nil;
        }
        AFREE(cd);
    } else {
        W("%s: No descriptor", __FUNCTION__);
    }
}

/* Resets camera device after capturing.
 * Since new capture request may require different frame dimensions we must
 * reset frame info cached in the capture window. The only way to do that would
 * be closing, and reopening it again. */
static void
_camera_device_reset(MacCameraDevice* cd)
{
    if (cd != NULL && cd->device) {
        [cd->device free];
        cd->device = [cd->device init];
    }
}

/*******************************************************************************
 *                     CameraDevice API
 ******************************************************************************/

CameraDevice*
camera_device_open(const char* name, int inp_channel)
{
    MacCameraDevice* mcd;

    mcd = _camera_device_alloc();
    if (mcd == NULL) {
        E("%s: Unable to allocate MacCameraDevice instance", __FUNCTION__);
        return NULL;
    }
    mcd->device = [[MacCamera alloc] init];
    if (mcd->device == nil) {
        E("%s: Unable to initialize camera device.", __FUNCTION__);
        return NULL;
    }
    return &mcd->header;
}

int
camera_device_start_capturing(CameraDevice* cd,
                              uint32_t pixel_format,
                              int frame_width,
                              int frame_height)
{
    MacCameraDevice* mcd;

    /* Sanity checks. */
    if (cd == NULL || cd->opaque == NULL) {
        E("%s: Invalid camera device descriptor", __FUNCTION__);
        return -1;
    }
    mcd = (MacCameraDevice*)cd->opaque;
    if (mcd->device == nil) {
        E("%s: Camera device is not opened", __FUNCTION__);
        return -1;
    }

    return [mcd->device start_capturing:frame_width:frame_height];
}

int
camera_device_stop_capturing(CameraDevice* cd)
{
    MacCameraDevice* mcd;

    /* Sanity checks. */
    if (cd == NULL || cd->opaque == NULL) {
        E("%s: Invalid camera device descriptor", __FUNCTION__);
        return -1;
    }
    mcd = (MacCameraDevice*)cd->opaque;
    if (mcd->device == nil) {
        E("%s: Camera device is not opened", __FUNCTION__);
        return -1;
    }

    /* Reset capture settings, so next call to capture can set its own. */
    _camera_device_reset(mcd);

    return 0;
}

int
camera_device_read_frame(CameraDevice* cd,
                         ClientFrameBuffer* framebuffers,
                         int fbs_num,
                         float r_scale,
                         float g_scale,
                         float b_scale,
                         float exp_comp)
{
    MacCameraDevice* mcd;

    /* Sanity checks. */
    if (cd == NULL || cd->opaque == NULL) {
        E("%s: Invalid camera device descriptor", __FUNCTION__);
        return -1;
    }
    mcd = (MacCameraDevice*)cd->opaque;
    if (mcd->device == nil) {
        E("%s: Camera device is not opened", __FUNCTION__);
        return -1;
    }

    return [mcd->device read_frame:framebuffers:fbs_num:r_scale:g_scale:b_scale:exp_comp];
}

void
camera_device_close(CameraDevice* cd)
{
    /* Sanity checks. */
    if (cd == NULL || cd->opaque == NULL) {
        E("%s: Invalid camera device descriptor", __FUNCTION__);
    } else {
        _camera_device_free((MacCameraDevice*)cd->opaque);
    }
}

int
enumerate_camera_devices(CameraInfo* cis, int max)
{
/* Array containing emulated webcam frame dimensions.
 * QT API provides device independent frame dimensions, by scaling frames
 * received from the device to whatever dimensions were requested for the
 * output device. So, we can just use a small set of frame dimensions to
 * emulate.
 */
static const CameraFrameDim _emulate_dims[] =
{
  /* Emulates 640x480 frame. */
  {640, 480},
  /* Emulates 352x288 frame (required by camera framework). */
  {352, 288},
  /* Emulates 320x240 frame (required by camera framework). */
  {320, 240},
  /* Emulates 176x144 frame (required by camera framework). */
  {176, 144}
};

    /* Obtain default video device. QT API doesn't really provide a reliable
     * way to identify camera devices. There is a QTCaptureDevice::uniqueId
     * method that supposedly does that, but in some cases it just doesn't
     * work. Until we figure out a reliable device identification, we will
     * stick to using only one (default) camera for emulation. */
    QTCaptureDevice* video_dev =
        [QTCaptureDevice defaultInputDeviceWithMediaType:QTMediaTypeVideo];
    if (video_dev == nil) {
        D("No web cameras are connected to the host.");
        return 0;
    }

    /* Obtain pixel format for the device. */
    NSArray* pix_formats = [video_dev formatDescriptions];
    if (pix_formats == nil || [pix_formats count] == 0) {
        E("Unable to obtain pixel format for the default camera device.");
        [video_dev release];
        return 0;
    }
    const uint32_t qt_pix_format = [[pix_formats objectAtIndex:0] formatType];
    [pix_formats release];

    /* Obtain FOURCC pixel format for the device. */
    cis[0].pixel_format = _QTtoFOURCC(qt_pix_format);
    if (cis[0].pixel_format == 0) {
        /* Unsupported pixel format. */
        E("Pixel format '%.4s' reported by the camera device is unsupported",
          (const char*)&qt_pix_format);
        [video_dev release];
        return 0;
    }

    /* Initialize camera info structure. */
    cis[0].frame_sizes = (CameraFrameDim*)malloc(sizeof(_emulate_dims));
    if (cis[0].frame_sizes != NULL) {
        cis[0].frame_sizes_num = sizeof(_emulate_dims) / sizeof(*_emulate_dims);
        memcpy(cis[0].frame_sizes, _emulate_dims, sizeof(_emulate_dims));
        cis[0].device_name = ASTRDUP("webcam0");
        cis[0].inp_channel = 0;
        cis[0].display_name = ASTRDUP("webcam0");
        cis[0].in_use = 0;
        [video_dev release];
        return 1;
    } else {
        E("Unable to allocate memory for camera information.");
        [video_dev release];
        return 0;
    }
}
