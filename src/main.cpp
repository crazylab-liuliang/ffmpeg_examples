// tutorial01.c
// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
// With updates from https://github.com/chelyaev/ffmpeg-tutorial
// Updates tested on:
// LAVC 54.59.100, LAVF 54.29.104, LSWS 2.1.101
// on GCC 4.7.2 in Debian February 2015

// A small sample program that shows how to use libavformat and libavcodec to
// read video from a file.
//
// Use
//
// gcc -o tutorial01 tutorial01.c -lavformat -lavcodec -lswscale -lz
//
// to build (assuming libavformat and libavcodec are correctly installed
// your system).
//
// Run using
//
// tutorial01 myvideofile.mpg
//
// to write the first five frames from "myvideofile.mpg" to disk in PPM
// format.

#include <vector>

using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <stdio.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
    FILE *pFile;
    char szFilename[256];
    int  y;
    
    // Open file
    sprintf(szFilename, "E:/ffmpeg_test/frames/frame%d.ppm", iFrame);
    pFile=fopen(szFilename, "wb");
    if(pFile==NULL)
        return;
    
    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);
    
    // Write pixel data
    for(y=0; y<height; y++)
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);

	unsigned char* color = (unsigned char*)pFrame->data;
    
    // Close file
    fclose(pFile);
}

static SwsContext* swsCtx = NULL;
static AVFrame*pFrameRGB = NULL;
static void yuv420p_to_rgb(AVFrame* frame420p, AVFrame* frameRGB)
{
	if (!swsCtx) {

		uint8_t *buffer = NULL;
		int numBytes;
		// Determine required buffer size and allocate buffer
		numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frame420p->width,
			frame420p->height, 1);
		buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

		//pFrameRGB->buf = buffer;
		pFrameRGB->width = frame420p->width;
		pFrameRGB->height = frame420p->height;
		pFrameRGB->format = AV_PIX_FMT_RGB24;
		int x = av_image_fill_arrays(pFrameRGB->data,pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, frame420p->width, frame420p->height, 1);

		swsCtx = sws_getContext(frame420p->width, frame420p->height, AVPixelFormat(frame420p->format), frame420p->width, frame420p->height, AV_PIX_FMT_RGB24,SWS_BILINEAR,NULL,NULL,NULL);
	}

	int result = sws_scale(swsCtx, (uint8_t const * const *)frame420p->data, frame420p->linesize, 0, frame420p->height, frameRGB->data, frameRGB->linesize);

	int a = 10;
}

static int frameCount = 0;

void decode_frame_from_packet(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
    char buf[1024];
    int ret;
    
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        

		yuv420p_to_rgb(frame, pFrameRGB);
		SaveFrame(pFrameRGB, pFrameRGB->width, pFrameRGB->height, frameCount);

        frameCount++;
        fprintf(stderr,"get frame --- %d\n", frameCount);
    }
}

int main() {
    // Initalizing these to NULL prevents segfaults!
    AVFormatContext   *pFormatCtx = NULL;
    int               videoStream;
    AVCodecContext    *pCodecCtx = NULL;
    AVCodec           *pCodec = NULL;
    AVFrame           *pFrame = NULL;
    AVPacket          packet;

	avformat_network_init();

    // Register all formats and codecs
    av_register_all();
    
    // Open video file
	if (avformat_open_input(&pFormatCtx, "http://albertlab-huanan.oss-cn-shenzhen.aliyuncs.com/Videos/spotmini.webm", NULL, NULL)!=0)
        return -1; // Couldn't open file
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return -1; // Couldn't find stream information
    
    // Dump information about file onto standard error
	//av_dump_format(pFormatCtx, 0, "http://albertlab-huanan.oss-cn-shenzhen.aliyuncs.com/Videos/spotmini.webm", 0);
    
    // Find the first video stream
    videoStream=-1;
    for(int i=0; i<pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
            videoStream=i;
            break;
        }
    if(videoStream==-1)
        return -1; // Didn't find a video stream
    
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
    
    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    
    avcodec_parameters_to_context(pCodecCtx,pFormatCtx->streams[videoStream]->codecpar);
    
    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
        return -1; // Could not open codec
    
    // Allocate video frame
    pFrame=av_frame_alloc();
	pFrameRGB = av_frame_alloc();
    
    // Read frames and save first five frames to disk
    // i=0;
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            decode_frame_from_packet(pCodecCtx, pFrame, &packet);
        }
    }
    
	av_packet_unref(&packet);
    
    // Free the YUV frame
    av_frame_free(&pFrame);
	av_frame_free(&pFrameRGB);
    
    //avcodec_free_context(&pCodecCtxOrig);
    avcodec_free_context(&pCodecCtx);
    
    // Close the codecs
    avcodec_close(pCodecCtx);
    
    // Close the video file
    avformat_close_input(&pFormatCtx);
    
    return 0;
}
