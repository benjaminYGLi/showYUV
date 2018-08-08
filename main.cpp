#include <cutils/memory.h>  
      
#include <unistd.h>  
#include <utils/Log.h>  
      
#include <binder/IPCThreadState.h>  
#include <binder/ProcessState.h>  
#include <binder/IServiceManager.h>  
      
#include <gui/Surface.h>  
#include <gui/SurfaceComposerClient.h>  
#include <gui/ISurfaceComposer.h>  
#include <ui/DisplayInfo.h>  
#include <ui/GraphicBuffer.h>  
#include <android/native_window.h>  
#include <window.h>

#define LOG_TAG "showYUV"

using namespace android;

void convertYUV420PToYV12(unsigned char *i, int size)
{
    int u_size = size/4;
    unsigned char buff[u_size];
    // copy U to buff
    memcpy(buff, i+size, u_size);
    // replace U with V
    memcpy(i+size, i+size+u_size, u_size);
    // copy buff to V
    memcpy(i+size+u_size, buff, u_size);
}

int main(int argc, char **argv) {
	if (argc < 5) {
		printf("example:");
		printf("\t showYUV filePath fileFormat WP*HP Layer");
		return -1;
	}

	// set up the thread-pool  
	sp<ProcessState> proc(ProcessState::self());  
	ProcessState::self()->startThreadPool();  

	// create a client to surfaceflinger  
	sp<SurfaceComposerClient> client = new SurfaceComposerClient();  
	sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(  
				ISurfaceComposer::eDisplayIdMain));  
	DisplayInfo dinfo;  
	// get the display width, height and etc. info
	status_t status = SurfaceComposerClient::getDisplayInfo(dtoken, &dinfo);  
	ALOGD("w=%d,h=%d,xdpi=%f,ydpi=%f,fps=%f,ds=%f\n",
			dinfo.w, dinfo.h, dinfo.xdpi, dinfo.ydpi, dinfo.fps, dinfo.density);  
	if (status)  
		return -1;  

	// create SurfaceControl
	sp<SurfaceControl> surfaceControl = client->createSurface(String8("showYUV"),
							dinfo.w, dinfo.h, PIXEL_FORMAT_RGBA_8888, 0);
                  
    /*************************get yuv data from file;****************************************/            
	const char *path = argv[1];

	int width,height;  
	if (strcmp(argv[2], "cif") == 0) {
		width = 352;
		height = 288;
	} else if (strcmp(argv[2], "qcif") == 0) {
		width = 176;
		height = 144;
	} else {
		printf("unsupported format, exit now\n");
		return -1;
	}

	int wp = 0;
	int hp = 0;
	sscanf(argv[3], "%d*%d", &wp, &hp);
	int layer = atoi(argv[4]);
	printf("wp: %d, hp: %d, layer: %d\n", wp, hp, layer);

	int size = width * height * 3/2;  
	unsigned char *data = new unsigned char[size];  

	FILE *fp = fopen(path,"rb");  
	if (fp == NULL) {
		ALOGE("read %s fail !\n", path);
		return -errno;
	}
    /*********************config surface*****************************************************/  
	SurfaceComposerClient::openGlobalTransaction();  
	surfaceControl->setLayer(layer);// set Z position
	surfaceControl->setPosition(wp, hp);// set the display position, offset from left-up
	surfaceControl->setSize(width, height);// set the display size
	SurfaceComposerClient::closeGlobalTransaction();  
	sp<Surface> surface = surfaceControl->getSurface();  

    /****************************************************************************************/    
	unsigned int *pBufferAddr;
	ANativeWindowBuffer *anb = NULL;
	sp<ANativeWindow> window(surface);
	ANativeWindow *nWindow = surface.get();

	status_t result = native_window_api_connect(nWindow, NATIVE_WINDOW_API_CPU);
	if (result != NO_ERROR) {
		ALOGE("ERROR: failed to do native_window_api_connect: %d", result);
		return result;
	}

	result = native_window_set_buffers_format(nWindow, HAL_PIXEL_FORMAT_YV12);
	if (result != NO_ERROR) {
		ALOGE("ERROR: failed to do native_window_set_buffers_format: %d", result);
		return result;
	}

	ALOGD("start to operate GraphicBuffer");
	do {
#ifdef USE_GRAPHICBUFFER
		// Transfer GraphicBuffer using ANW function
		int fenceFd = -1;
		int err = window->dequeueBuffer(window.get(), &anb, &fenceFd);
		if (err != 0) {
			ALOGE("ERROR: dequeueBuffer returned error: %d\n", err);
			return -1;
		}

		uint8_t *img = NULL;

#ifdef ANDROID8
        sp<GraphicBuffer> buffer(GraphicBuffer::from(anb));
#endif

#ifdef ANDROID7
        sp<GraphicBuffer> buffer = new GraphicBuffer(anb, false);
#endif

        uint32_t stride = buffer->getStride();

        err = buffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void **)(&img));
        if (err != 0) {
            ALOGE("ERROR: lock failed\n");
            return -1;
        }
#else
        ANativeWindow_Buffer outBuffer;
        if (NO_ERROR != surface->lock(&outBuffer, NULL)) {
            printf("error with lock Surface outBuffer\n");
            return -1;
        }
        uint8_t* img = reinterpret_cast<uint8_t*>(outBuffer.bits);
#endif

        fread(data, size, 1, fp);
        convertYUV420PToYV12(data, width * height);
        memcpy(img, data, size);

#ifdef USE_GRAPHICBUFFER
        err = buffer->unlock();
        if (err != 0) {
            ALOGE("ERROR: unlock failed\n");
            return -1;
        }

        if ((err = window->queueBuffer(window.get(), anb, -1)) != 0) {
            ALOGE("ERROR: queueBuffer returned error: %d\n", err);
            return -2;
        }
#else
        if (NO_ERROR != surface->unlockAndPost()) {
            printf("error with unlockAndPost\n");
            return -3;
        }
#endif
	} while (feof(fp) == 0);

	ALOGD("close yuv file\n");
	fclose(fp);

	if (anb != NULL) {
		window->cancelBuffer(nWindow, anb, -1);
		anb = NULL;
	}

	result = native_window_api_disconnect(nWindow, NATIVE_WINDOW_API_CPU);
	if (result != NO_ERROR) {
		ALOGE("ERROR: failed to do native_window_api_disconnect: %d", result);
		return result;
	}

	IPCThreadState::self()->joinThreadPool();
	IPCThreadState::self()->stopProcess();  
	return 0;  
}
