#include <stdio.h>
#include <fmt/format.h>

// libavformat deals with the capsule of video

// if your code c++, you can include c headers with an extern encapsulation
extern "C"
{
#include <libavformat/avformat.h>
}

struct HAVFormat {
	AVFormatContext *ctx = nullptr;
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

int main()
{	
	char *filename = "../fuck.wmv";

	HAVFormat format(filename);

	// loop streams and dump info
	for (int i = 0; i < format.get_nb_stream(); i++)
	{
		av_dump_format(format.ctx, i, filename, false);		
	}


	return 0;
}
