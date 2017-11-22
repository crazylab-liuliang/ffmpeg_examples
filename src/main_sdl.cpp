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

#include <assert.h>
#include <vector>
#include <SDL.h>
#include <thread>
#include "QRingBuffer.h"
#include <vector>

using namespace std;

#undef main

#include <SDL_thread.h>

using namespace std;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <stdio.h>

#include <Windows.h>

SDL_Surface* screen = NULL;
SDL_Overlay* bmp = NULL;

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
		numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frame420p->width, frame420p->height, 1);
		buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

		//pFrameRGB->buf = buffer;
		pFrameRGB->width = frame420p->width;
		pFrameRGB->height = frame420p->height;
		pFrameRGB->format = AV_PIX_FMT_RGB24;
		int x = av_image_fill_arrays(pFrameRGB->data,pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, frame420p->width, frame420p->height, 1);

		swsCtx = sws_getContext(frame420p->width, frame420p->height, AVPixelFormat(frame420p->format), frame420p->width, frame420p->height, AV_PIX_FMT_RGB24,SWS_BILINEAR,NULL,NULL,NULL);
	}

	int result = sws_scale(swsCtx, (uint8_t const * const *)frame420p->data, frame420p->linesize, 0, frame420p->height, frameRGB->data, frameRGB->linesize);
}

static int frameCount = 0;

int decode_frame_from_packet(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
	static DWORD startTime = GetTickCount();

    int ret;
    
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return -1;
    }
    

	int frameNumPerPkt = 0;

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return -1;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return -1;
        }
        
		double pts = 0;
		if (pkt->dts != AV_NOPTS_VALUE)
		{
			pts = av_frame_get_best_effort_timestamp(frame);
		}
		else
		{
			pts = 0;
		}

		AVRational timeBase = dec_ctx->time_base;
		AVRational frameRate = av_inv_q(dec_ctx->framerate);

		pts *= av_q2d(frameRate);


		/*DWORD elapsedTime = GetTickCount() - startTime;
		while (elapsedTime < pts)
		{
			Sleep(5);
			elapsedTime = GetTickCount() - startTime;
		}
		*/


		SDL_LockYUVOverlay(bmp);

		bmp->pixels[0] = frame->data[0];
		bmp->pixels[1] = frame->data[2];
		bmp->pixels[2] = frame->data[1];
		bmp->pitches[0] = frame->linesize[0];
		bmp->pitches[1] = frame->linesize[1];
		bmp->pitches[2] = frame->linesize[2];

		SDL_UnlockYUVOverlay(bmp);

		//yuv420p_to_rgb(frame, pFrameRGB);
		//SaveFrame(pFrameRGB, pFrameRGB->width, pFrameRGB->height, frameCount);

        frameCount++;
        fprintf(stderr,"get frame --- %d\n", frameCount);

		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = frame->width;
		rect.h = frame->height;

		SDL_DisplayYUVOverlay(bmp, &rect);

		frameNumPerPkt++;
    }

	return 0;

	int a = 10;
}

AVCodecContext*	  aCodecCtx = NULL;
AVCodecParserContext* parser = NULL;
AVCodec*		  aCodec = NULL;

typedef struct PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;

PacketQueue audioq;

int quit = 0;


int audio_decode_frame(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
	int ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		return -1;
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			return -1;
		}
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding\n");
			return -1;
		}

		int dataSize = av_samples_get_buffer_size(NULL, dec_ctx->channels, frame->nb_samples, aCodecCtx->sample_fmt, 1);



		int sample_bytes = av_get_bytes_per_sample(dec_ctx->sample_fmt);
		for (int i = 0; i < frame->nb_samples; i++)
		{
			for (int ch = 0; ch < dec_ctx->channels; ch++)
			{

				uint8_t* bufferStart = frame->data[ch] + sample_bytes * i;
				int a = 10;
			}
		}

		int a = 10;
	}

	return 1024;
}


#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

void audio_callback(void *userdata, Uint8 *stream, int len) {

	AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
	int len1, audio_size;

	static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
	static unsigned int audio_buf_size = 0;
	static unsigned int audio_buf_index = 0;

	while (len > 0) {
		if (audio_buf_index >= audio_buf_size) {
			/* We have already sent all our data; get more */
			audio_size = 0;// audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
			if (audio_size < 0) {
				/* If error, output silence */
				audio_buf_size = 1024; // arbitrary?
				memset(audio_buf, 0, audio_buf_size);
			}
			else {
				audio_buf_size = audio_size;
			}
			audio_buf_index = 0;
		}
		len1 = audio_buf_size - audio_buf_index;
		if (len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
		len -= len1;
		stream += len1;
		audio_buf_index += len1;
	}
}

#include "easywsclient.hpp"

int wsBufferIdx = 0;
vector<uint8_t> wsBuffer; 
void handle_ws_message(const std::vector<uint8_t>& message)
{
	for (int i = 0; i < message.size(); i++)
	{
		wsBuffer.push_back(message[i]);

		//wsBuffer.write(message.data(), message.size());
	}
}



int fill_iobuffer(void* opaque, uint8_t* buf, int bufsize) {
	
	int true_size = min(bufsize, wsBuffer.size()-wsBufferIdx);
	for (int i = 0; i < true_size; i++)
	{
		buf[i] = wsBuffer[wsBufferIdx];
		wsBufferIdx++;
		//wsBuffer.read(buf, true_size);

		//buf[i] = wsBuffer.front();
		//wsBuffer.pop();
	}

	return true_size > 0 ? true_size : -1;

	return -1;
}

void wsThreadFunc()
{
	static easywsclient::WebSocket::pointer ws = easywsclient::WebSocket::from_url("ws://124.243.220.24:10002/camera_0");
	while (ws->getReadyState() != easywsclient::WebSocket::CLOSED)
	{
		ws->poll();
		ws->dispatchBinary(handle_ws_message);
	}
}


int main() {



#ifdef _WIN32

	wsBuffer.reserve(512 * 1024 * 1024);

	INT rc;
	WSADATA wsaData;
	rc = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (rc)
	{
		printf("WSAStartup Failed\n");
		return -1;
	}
#endif

	std::thread th(wsThreadFunc);



	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

    // Initalizing these to NULL prevents segfaults!
    AVFormatContext   *pFormatCtx = NULL;
    int               videoStream;
    AVCodecContext    *pCodecCtx = NULL;
    AVCodec           *pCodec = NULL;
    AVFrame           *pFrame = NULL;
    AVPacket*          packet;

	avformat_network_init();

    // Register all formats and codecs
    av_register_all();
    
	packet = av_packet_alloc();

	unsigned char* iobuffer = (unsigned char*)av_malloc(32768);
	AVIOContext* avio = avio_alloc_context(iobuffer, 32768, 0, NULL, fill_iobuffer, NULL, NULL);
	pFormatCtx = avformat_alloc_context();
	pFormatCtx->pb = avio;

	Sleep(1000);

    // Open video file
	while (avformat_open_input(&pFormatCtx, ""/* "e:/sixwheel.mp4"*/, NULL, NULL) != 0)
	{
		Sleep(1000);
	}
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return -1; // Couldn't find stream information
    
    // Dump information about file onto standard error
	//av_dump_format(pFormatCtx, 0, "http://albertlab-huanan.oss-cn-shenzhen.aliyuncs.com/Videos/spotmini.webm", 0);
    
    // Find the first video stream
    videoStream=-1;
	int audioStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream<0) {
			videoStream = i;
		}

		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream<0) {
			audioStream = i;
		}
	}

    if(videoStream==-1)
        return -1; // Didn't find a video stream

	//if (audioStream == -1)
	//	return -1;

	//aCodec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
	//if (aCodec == NULL)
	//	return -1;

	//parser = av_parser_init(pFormatCtx->streams[audioStream]->codecpar->codec_id);
	//if (!parser)
	//	return -1;

	//aCodecCtx = avcodec_alloc_context3(aCodec);

	//avcodec_parameters_to_context(aCodecCtx, pFormatCtx->streams[audioStream]->codecpar);

	//if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0)
	//	return -1;

	//----------------------------------------------
    
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
    
    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    

	AVCodecParameters * codecPar = pFormatCtx->streams[videoStream]->codecpar;


    avcodec_parameters_to_context(pCodecCtx,pFormatCtx->streams[videoStream]->codecpar);
    
    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
        return -1; // Could not open codec
    
    // Allocate video frame
    pFrame=av_frame_alloc();
	pFrameRGB = av_frame_alloc();

	AVFrame* aFrame = av_frame_alloc();


	screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
	if (!screen)
		return -1;

	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height, SDL_YV12_OVERLAY, screen);


    // Read frames and save first five frames to disk
    // i=0;
	while (true) {
		if (av_read_frame(pFormatCtx, packet) >= 0) {
			// Is this a packet from the video stream?
			if (packet->stream_index == videoStream) {
				Sleep(50);
				decode_frame_from_packet(pCodecCtx, pFrame, packet);
			}
			if (packet->stream_index == audioStream) {
				//audio_decode_frame(aCodecCtx, aFrame, &packet);
			}
		}
	}
    
	av_packet_unref(packet);
    
    // Free the YUV frame
    av_frame_free(&pFrame);
	av_frame_free(&pFrameRGB);
    
    //avcodec_free_context(&pCodecCtxOrig);
    avcodec_free_context(&pCodecCtx);
    
    // Close the codecs
    avcodec_close(pCodecCtx);
    
    // Close the video file
    avformat_close_input(&pFormatCtx);

	//delete ws;
	WSACleanup();
    
    return 0;
}
