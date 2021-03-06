/*
* Copyright (c) 2001 Fabrice Bellard
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

/**
* @file
* libavcodec API use example.
*
* @example decoding_encoding.c
* Note that libavcodec only handles codecs (MPEG, MPEG-4, etc...),
* not file formats (AVI, VOB, MP4, MOV, MKV, MXF, FLV, MPEG-TS, MPEG-PS, etc...).
* See library 'libavformat' for the format handling
*/
#include "stdafx.h"
#pragma warning(disable:4996)

#include <math.h>
#include <windows.h>

#include "ScreenCapture.h"


extern "C"
{
	#include "libavutil/opt.h"
	#include "libavcodec/avcodec.h"
	#include "libavutil/channel_layout.h"
	#include "libavutil/common.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/mathematics.h""
	#include "libavutil/samplefmt.h"
	#include "libswScale/swscale.h"
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swScale.lib")


#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

/*
* Video encoding example
*/
static void video_encode_example(const char *filename, AVCodecID codec_id)
{
	AVCodec *codec;
	AVCodecContext *c = NULL;
	int i, ret, x, y, got_output;
	FILE *f;
	AVFrame *frame;
	AVPacket pkt;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };

	printf("Encode video file %s\n", filename);

	/* find the video encoder */
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	/* put sample parameters */
	c->bit_rate = 1200000;
	c->width = 1920;                                        // resolution must be a multiple of two (1280x720),(1900x1080),(720x480)
	c->height = 1080;
	c->time_base.num = 1;                                   // framerate numerator
	c->time_base.den = 25;                                  // framerate denominator
	c->gop_size = 10;                                       // emit one intra frame every ten frames
	c->max_b_frames = 1;                                    // maximum number of b-frames between non b-frames
	c->keyint_min = 1;                                      // minimum GOP size
	c->i_quant_factor = (float)0.71;                        // qscale factor between P and I frames
	c->b_frame_strategy = 20;                               ///// find out exactly what this does
	c->qcompress = (float)0.6;                              ///// find out exactly what this does
	c->qmin = 20;                                           // minimum quantizer
	c->qmax = 51;                                           // maximum quantizer
	c->max_qdiff = 4;                                       // maximum quantizer difference between frames
	c->refs = 4;                                            // number of reference frames
	c->trellis = 1;                                         // trellis RD Quantization
	c->pix_fmt = AV_PIX_FMT_YUV420P;                           // universal pixel format for video encoding
	c->codec_id = AV_CODEC_ID_H264;
	c->codec_type = AVMEDIA_TYPE_VIDEO;


	if (codec_id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	frame->format = c->pix_fmt;
	frame->width = c->width;
	frame->height = c->height;

	/* the image can be allocated by any means and av_image_alloc() is
	* just the most convenient way if av_malloc() is to be used */
	ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
		c->pix_fmt, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate raw picture buffer\n");
		exit(1);
	}

	/* encode 10 second of video */
	for (i = 0; i < 250; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL;    // packet data will be allocated by the encoder
		pkt.size = 0;

		fflush(stdout);
		int screenWidth = 1920;
		int screenHeight = 1080;
		RGBQUAD *pPixels = getBitmap(screenWidth, screenHeight);
		

		int nbytes = avpicture_get_size(AV_PIX_FMT_YUV420P, c->width, c->height);                                      // allocating outbuffer
		uint8_t* outbuffer = (uint8_t*)av_malloc(nbytes*sizeof(uint8_t));

		AVFrame* inpic = av_frame_alloc();                                                                     // mandatory frame allocation

		frame->pts = (int64_t)((float)i * (1000.0 / ((float)(c->time_base.den))) * 90);                              // setting frame pts
		avpicture_fill((AVPicture*)inpic, (uint8_t*)pPixels, AV_PIX_FMT_RGB32, c->width, c->height);                   // fill image with input screenshot
		avpicture_fill((AVPicture*)frame, outbuffer, AV_PIX_FMT_YUV420P, c->width, c->height);                        // clear output picture for buffer copy
		av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 1);

		inpic->data[0] += inpic->linesize[0] * (screenHeight - 1);                                                      // flipping frame
		inpic->linesize[0] = -inpic->linesize[0];                                                                   // flipping frame

		struct SwsContext* fooContext = sws_getContext(screenWidth, screenHeight, AV_PIX_FMT_RGB32, c->width, c->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
		sws_scale(fooContext, inpic->data, inpic->linesize, 0, c->height, frame->data, frame->linesize);          // converting frame size and format

		delete[] pPixels;
		frame->pts = i;

		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_packet_unref(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		fflush(stdout);

		ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_packet_unref(&pkt);
		}
	}

	/* add sequence end code to have a real MPEG file */
	fwrite(endcode, 1, sizeof(endcode), f);
	fclose(f);

	avcodec_free_context(&c);
	av_freep(&frame->data[0]);
	av_frame_free(&frame);
	printf("\n");
}

/*
* Video decoding example
*/

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
	char *filename)
{
	FILE *f;
	int i;

	f = fopen(filename, "w");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}

static int decode_write_frame(const char *outfilename, AVCodecContext *avctx,
	AVFrame *frame, int *frame_count, AVPacket *pkt, int last)
{
	int len, got_frame;
	char buf[1024];

	len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
	if (len < 0) {
		fprintf(stderr, "Error while decoding frame %d\n", *frame_count);
		return len;
	}
	if (got_frame) {
		printf("Saving %sframe %3d\n", last ? "last " : "", *frame_count);
		fflush(stdout);

		/* the picture is allocated by the decoder, no need to free it */
		snprintf(buf, sizeof(buf), outfilename, *frame_count);
		pgm_save(frame->data[0], frame->linesize[0],
			frame->width, frame->height, buf);
		(*frame_count)++;
	}
	if (pkt->data) {
		pkt->size -= len;
		pkt->data += len;
	}
	return 0;
}

static void video_decode_example(const char *outfilename, const char *filename)
{
	AVCodec *codec;
	AVCodecContext *c = NULL;
	int frame_count;
	FILE *f;
	AVFrame *frame;
	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	AVPacket avpkt;

	av_init_packet(&avpkt);

	/* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	printf("Decode video file %s to %s\n", filename, outfilename);

	/* find the MPEG-1 video decoder */
	codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	if (codec->capabilities & AV_CODEC_CAP_TRUNCATED)
		c->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

											 /* For some codecs, such as msmpeg4 and mpeg4, width and height
											 MUST be initialized there because this information is not
											 available in the bitstream. */

											 /* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}

	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	frame_count = 0;
	for (;;) {
		avpkt.size = fread(inbuf, 1, INBUF_SIZE, f);
		if (avpkt.size == 0)
			break;

		/* NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
		and this is the only method to use them because you cannot
		know the compressed data size before analysing it.
		BUT some other codecs (msmpeg4, mpeg4) are inherently frame
		based, so you must call them with all the data for one
		frame exactly. You must also initialize 'width' and
		'height' before initializing them. */

		/* NOTE2: some codecs allow the raw parameters (frame size,
		sample rate) to be changed at any frame. We handle this, so
		you should also take care of it */

		/* here, we use a stream based decoder (mpeg1video), so we
		feed decoder and see if it could decode a frame */
		avpkt.data = inbuf;
		while (avpkt.size > 0)
			if (decode_write_frame(outfilename, c, frame, &frame_count, &avpkt, 0) < 0)
				exit(1);
	}

	/* Some codecs, such as MPEG, transmit the I- and P-frame with a
	latency of one frame. You must do the following to have a
	chance to get the last frame of the video. */
	avpkt.data = NULL;
	avpkt.size = 0;
	decode_write_frame(outfilename, c, frame, &frame_count, &avpkt, 1);

	fclose(f);

	avcodec_free_context(&c);
	av_frame_free(&frame);
	printf("\n");
}

int main(int argc, char **argv)
{
	const char *output_type;

	/* register all the codecs */
	avcodec_register_all();
	video_encode_example("test.h264", AV_CODEC_ID_H264);


	return 0;
}