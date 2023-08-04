#include <stdio.h>
#include <fmt/format.h>
#include <optional>
#include <string>
#include <unordered_map>

extern "C"
{
	// include format and codec headers
#include <libavformat\avformat.h>
#include <libavcodec\avcodec.h>
#include <sdl2\SDL.h>
}

#undef main

// define sdl variables globally, because we are writing c style
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Rect r;


void displayFrame(AVFrame * frame) {
	// pass frame data to texture then copy texture to renderer and present renderer
	SDL_UpdateYUVTexture(texture, &r, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}


void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
	//send packet to decoder
	int ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		exit(1);
	}
	while (ret >= 0) {
		// receive frame from decoder
		// we may receive multiple frames or we may consume all data from decoder, then return to main loop
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			// something wrong, quit program
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}
		printf("saving frame %3d\n", dec_ctx->frame_number);
		fflush(stdout);
		

		// display frame on sdl window
		displayFrame(frame);
	}
}


int initSDL(AVCodecContext *codec_ctx)
{
	SDL_Window *window = NULL;

	// init SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "could not init sdl %s\n", SDL_GetError());
		return -1;
	}

	//create sdl window
	window = SDL_CreateWindow("Preview", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, codec_ctx->width, codec_ctx->height, 0);
	if (!window)
	{
		fprintf(stderr, "could not create sdl window \n");
		return -1;
	}

	// specify rectangle position and scale
	r.x = 0;
	r.y = 0;
	r.w = codec_ctx->width;
	r.h = codec_ctx->height;

	// create sdl renderer
	renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer)
	{
		fprintf(stderr, "could not create sdl renderer \n");
		return -1;
	}

	// create texture
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, codec_ctx->width, codec_ctx->height);
	if (!texture)
	{
		fprintf(stderr, "could not create sdl texture \n");
		return -1;
	}

	return 0;
}

struct HAVFormat {
	AVFormatContext *ctx = nullptr;
	std::optional<int> video_stream_id = std::nullopt;
	std::unordered_map<std::string, std::string> tags;
	HAVFormat(const char* filename) {
		//fill format context with file info
		if (auto ret = avformat_open_input(&ctx, filename, NULL, NULL)) {
			fmt::print("Cannot open file {}\n", filename);
			exit(ret);
		}
		// get streams info from format context
		if (avformat_find_stream_info(ctx, NULL) < 0) {
			fmt::print("Cannot find stream information in {}\n", filename);
			exit(-1);
		}
		// get video stream index
		for (int i = 0; i < get_nb_stream(); i++) {
			if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				video_stream_id = i;
				break;
			}
		}
		AVDictionaryEntry *tag = nullptr;
		while (tag = av_dict_get(ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)) {
			tags[tag->key] = tag->value;
		}
	}
	unsigned int get_nb_stream() {
		return ctx->nb_streams;
	}
	~HAVFormat() {
		// close format context
		if (ctx) {
			avformat_close_input(&ctx);
		}
	}
};

struct HAVCodec {
	AVCodecContext *ctx = nullptr;
	const AVCodec *codec = nullptr;
	HAVCodec(const AVCodecParameters *par) {
		//alloc memory for codec context
		ctx = avcodec_alloc_context3(NULL);
		// retrieve codec params from format context
		if (avcodec_parameters_to_context(ctx, par) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot get codec parameters\n");
			exit(-1);
		}
		
		// find decoding codec
		codec = avcodec_find_decoder(ctx->codec_id);
		if (codec == nullptr) {
			av_log(NULL, AV_LOG_ERROR, "No decoder found\n");
			exit(-1);
		}

		// try to open codec
		if (avcodec_open2(ctx, codec, NULL) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
			exit(-1);
		}
	}
	~HAVCodec() {
		// close context
		if (ctx) {
			avcodec_close(ctx);
		}
	}
};

struct HAVPacket {
	AVPacket *pkt = nullptr;
	HAVPacket() {
		pkt = av_packet_alloc();
		av_init_packet(pkt);
		if (!pkt) {
			av_log(NULL, AV_LOG_ERROR, "Cannot init packet\n");
			exit(-1);
		}
	}
	int read_frame(AVFormatContext *s) {
		return av_read_frame(s, pkt);
	}
	void unref() {
		av_packet_unref(pkt);
	}
	~HAVPacket() {
		av_packet_free(&pkt);
	}
};

struct HAVFrame {
	AVFrame *frame = nullptr;
	HAVFrame() {
		frame = av_frame_alloc();
		if (!frame) {
			av_log(NULL, AV_LOG_ERROR, "Cannot init frame\n");
			exit(-1);
		}
	}
	~HAVFrame() {
		av_frame_free(&frame);
	}
};

int main() {
	int ret;

	FILE *fin = NULL;

	// declare format and codec contexts, also codec for decoding
	char *filename = "../fuck.mp4";

	HAVFormat format(filename);

	if (!format.video_stream_id.has_value()) {
		av_log(NULL, AV_LOG_ERROR, "No video stream\n");
		exit(-1);
	}

	// dump video stream info
	av_dump_format(format.ctx, format.video_stream_id.value(), filename, false);

	HAVCodec havcodec(format.ctx->streams[format.video_stream_id.value()]->codecpar);

	fmt::print("\nDecoding codec is : {}\n", havcodec.codec->name);


	//open input file
	fin = fopen(filename, "rb");
	if (!fin) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		exit(-1);
	}

	// init sdl 
	if (initSDL(havcodec.ctx) < 0) {
		av_log(NULL, AV_LOG_ERROR, "init sdl failed\n");
		exit(-1);
	}

	HAVPacket pkt;
	HAVFrame frame;
	// read an encoded packet from file
	while (pkt.read_frame(format.ctx) >= 0) {
		// if packet data is video data then send it to decoder
		if (pkt.pkt->stream_index == format.video_stream_id.value()) {
			decode(havcodec.ctx, frame.frame, pkt.pkt);
		}

		// release packet buffers to be allocated again
		pkt.unref();
	}

	//flush decoder
	decode(havcodec.ctx, frame.frame, NULL);


	if (fin)
		fclose(fin);

	return 0;
}
