#include<include/SoftwareRenderer.h>  
      
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

using namespace android;

int main(void){  
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
	printf("w=%d,h=%d,xdpi=%f,ydpi=%f,fps=%f,ds=%f\n",   
			dinfo.w, dinfo.h, dinfo.xdpi, dinfo.ydpi, dinfo.fps, dinfo.density);  
	if (status)  
		return -1;  
	// create SurfaceControl
	sp<SurfaceControl> surfaceControl = client->createSurface(String8("showYUV"),
							dinfo.w, dinfo.h, PIXEL_FORMAT_RGBA_8888, 0);
                  
    /*************************get yuv data from file;****************************************/            
	int width,height;  
	width = 352;  
	height = 288;  
	int size = width * height * 3/2;  
	unsigned char *data = new unsigned char[size];  
	const char *path = "/system/usr/yuv_352_288.yuv";

	FILE *fp = fopen(path,"rb");  
	if (fp == NULL) {
		printf("read %s fail !\n", path);  
		return -errno;
	}
    /*********************config surface*****************************************************/  
	SurfaceComposerClient::openGlobalTransaction();  
	surfaceControl->setLayer(100000);// set Z position
	surfaceControl->setPosition(100, 100);// set the display position, offset from left-up
	surfaceControl->setSize(width, height);// set the display size
	SurfaceComposerClient::closeGlobalTransaction();  
	sp<Surface> surface = surfaceControl->getSurface();  

    /****************************************************************************************/    
	unsigned int *pBufferAddr;
	ANativeWindowBuffer *anb;
	sp<ANativeWindow> window(surface);
	ANativeWindow *nWindow = surface.get();

	status_t result = native_window_api_connect(nWindow, NATIVE_WINDOW_API_CPU);
	if (result != NO_ERROR) {
		printf("ERROR: failed to do native_window_api_connect: %d", result);
		return result;
	}

	result = native_window_set_buffers_format(nWindow, HAL_PIXEL_FORMAT_YV12);
	if (result != NO_ERROR) {
		printf("ERROR: failed to do native_window_set_buffers_format: %d", result);
		return result;
	}

	do {
		// Transfer GraphicBuffer using ANW function
		int fenceFd = -1;
		int err = window->dequeueBuffer(window.get(), &anb, &fenceFd);
		if (err != 0) {
			printf("ERROR: dequeueBuffer returned error: %d\n", err);
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
            printf("ERROR: lock failed\n");
            return -1;
        }

        fread(data, size, 1, fp);
        memcpy(img, data, size);

        err = buffer->unlock();
        if (err != 0) {
            printf("ERROR: unlock failed\n");
            return -1;
        }

        if ((err = window->queueBuffer(window.get(), anb, -1)) != 0) {
            printf("ERROR: queueBuffer returned error: %d\n", err);
            return -2;
        } 
	} while (feof(fp) == 0);

	printf("close yuv file\n");
	fclose(fp);

	if (anb != NULL) {
		window->cancelBuffer(nWindow, anb, -1);
		anb = NULL;
	}

	result = native_window_api_disconnect(nWindow, NATIVE_WINDOW_API_CPU);
	if (result != NO_ERROR) {
		printf("ERROR: failed to do native_window_api_disconnect: %d", result);
		return result;
	}

	IPCThreadState::self()->joinThreadPool();
	IPCThreadState::self()->stopProcess();  
	return 0;  
}
