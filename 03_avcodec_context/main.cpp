#include <stdio.h>
#include <fmt/format.h>
#include <optional>
#include <string>
#include <unordered_map>

extern "C"
{
	// include format and codec headers
#include <libavformat\avformat.h>
#include <libavutil\dict.h>
#include <libavcodec\avcodec.h>

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

int main() {
	// declare format and codec contexts, also codec for decoding
	char *filename = "../fuck.wmv";

	HAVFormat format(filename);

	if (!format.video_stream_id.has_value()) {
		av_log(NULL, AV_LOG_ERROR, "No video stream\n");
		exit(-1);
	}

	// dump video stream info
	av_dump_format(format.ctx, format.video_stream_id.value(), filename, false);

	HAVCodec havcodec(format.ctx->streams[format.video_stream_id.value()]->codecpar);

	fmt::print("\nDecoding codec is : {}\n", havcodec.codec->name);

	return 0;
}