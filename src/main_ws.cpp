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

static SwsContext* swsCtx = NULL;
static AVFrame*pFrameRGB = NULL;

static int frameCount = 0;

void decode_frame_from_packet(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
	static DWORD startTime = GetTickCount();

    int ret;
    
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
    

	int frameNumPerPkt = 0;

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
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


		DWORD elapsedTime = GetTickCount() - startTime;
		while (elapsedTime < pts)
		{
			Sleep(5);
			elapsedTime = GetTickCount() - startTime;
		}



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


#define INBUF_SIZE 4096


int main() {



#ifdef _WIN32

	wsBuffer.reserve(128 * 1024 * 1024);

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
	AVCodecParserContext* parser;
    int               videoStream;
    AVCodecContext    *pCodecCtx = NULL;
    AVCodec           *pCodec = NULL;
    AVFrame           *pFrame = NULL;
    AVPacket*          packet;

	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	uint8_t* data;
	size_t   data_size;
	int		 ret;

	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	avformat_network_init();

    // Register all formats and codecs
    av_register_all();
    
	packet = av_packet_alloc();
  
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO/*pFormatCtx->streams[videoStream]->codecpar->codec_id*/);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }
    

	parser = av_parser_init(pCodec->id);
	if (!parser)
	{
		fprintf(stderr, "parser not found\n");
		return -1;
	}


    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    
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
    while(true) {
		if (wsBuffer.size() - wsBufferIdx > INBUF_SIZE)
		{
			data_size = fill_iobuffer(NULL, inbuf, INBUF_SIZE);
			if(data_size<0)
				break;
		}

		data = inbuf;

		while (data_size > 0)
		{
			ret = av_parser_parse2(parser, pCodecCtx, &packet->data, &packet->size, data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

			if (ret < 0)
			{
				fprintf(stderr, "error while parsing\n");
				exit(1);
			}

			data += ret;
			data_size -= ret;

			if (packet->size)
			{
				decode_frame_from_packet(pCodecCtx, pFrame, packet);
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

	//delete ws;
	WSACleanup();
    
    return 0;
}
