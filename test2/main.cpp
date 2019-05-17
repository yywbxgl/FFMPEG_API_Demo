#include <stdio.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

//在本教程中，我们将打开一个文件，从其中的视频流中读取，我们要做的是将帧写入PPM文件。
//ffmpeg version : 4.1
//编译命令 cmake MakeLists.text; make

int main(int argc, char *argv[]) 
{
    //这将在库中注册所有可用的文件格式和编解码器，以便在打开具有相应格式/编解码器的文件时自动使用它们。
    //注意，您只需要调用av_register_all()一次，因此我们在main()中进行了调用。
    //如果您愿意，可以只注册特定的文件格式和编解码器，但是通常没有必要这样做。
    av_register_all();

    AVFormatContext *pFormatCtx = NULL;

    //我们从第一个参数得到文件名。这个函数读取文件头并在我们给出的AVFormatContext结构中存储有关文件格式的信息。
    //最后三个参数用于指定文件格式、格式选项，但是通过将其设置为NULL或0,libavformat将自动检测这些参数。
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
        return -1; // Couldn't open file


    //avformat_open_input这个函数只看头部，所以接下来我们需要检查文件中的流信息。
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return -1; // Couldn't find stream information

    //打印pFormatCtx有什么  用于调试
    printf("-----------sun-----------------\n");
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    printf("-----------sun-----------------\n");

    //现在pFormatCtx->流只是一个指针数组，大小为pFormatCtx->nb_streams，让我们遍历它，直到找到一个视讯流。
    int i;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;

    // Find the first video stream
    int videoStream=-1;
    for(i=0; i < pFormatCtx->nb_streams; i++){
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
            videoStream=i;
            break;
        }
    }
    if(videoStream==-1)
        return -1; // Didn't find a video stream


    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    //流关于编解码器的信息在我们所谓的“编解码器上下文中”。它包含了流使用的所有关于编解码器的信息，现在我们有一个指向它的指针。但我们仍然需要找到实际的编解码器并打开它:    
    AVCodec *pCodec = NULL;
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }

    // Open codec
    AVDictionary    *optionsDict = NULL;
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
        return -1; // Could not open codec

    // Allocate video frame
    //由于我们计划输出PPM文件，这些文件存储在24位RGB中，所以我们必须将帧从其原始格式转换为RGB。
    //ffmpeg将为我们做这些转换。对于大多数项目(包括我们的项目)，我们希望将初始框架转换为特定的格式。现在让我们为转换后的帧分配一个帧。
    // Allocate an AVFrame structure
    AVFrame *pFrame = av_frame_alloc();
    AVFrame *pFrameRGB = av_frame_alloc();
    if(pFrameRGB==NULL)
        return -1;

    //即使我们已经分配了帧，当我们转换原始数据时，仍然需要一个放置原始数据的位置。我们使用avpicture_get_size来获得我们需要的大小，并手动分配空间:
    uint8_t *buffer = NULL;
    int numBytes;
    // Determine required buffer size and allocate buffer
    numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
                                pCodecCtx->height);
    //av_malloc是ffmpeg的malloc，它只是一个简单的malloc包装器，用于确保内存地址对齐等等。它不能防止内存泄漏、双重释放或其他malloc问题。
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));


    //现在，我们使用avpicture_fill将帧与新分配的缓冲区关联起来。
    //关于AVPicture cast: AVPicture struct是AVFrame struct的一个子集——AVFrame struct的开头与AVPicture struct相同。
    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                    pCodecCtx->width, pCodecCtx->height);

    //Finally! Now we're ready to read from the stream!
    //我们要做的是通过读取数据包来读取整个视频流，并将其解码到帧中，一旦帧完成，我们将转换并保存它。
    struct SwsContext *sws_ctx = NULL;
    int frameFinished;
    AVPacket packet;

	//初始化SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
	
    //打开SDL画面框用于显示
    SDL_Surface *screen;
    screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
    if(!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        return -1;
    }

    //现在我们在屏幕上创建一个YUV覆盖层，这样我们就可以输入视频到它，并设置我们的SWSContext将图像数据转换为YUV420:
    SDL_Overlay  *bmp = NULL;
    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                            SDL_YV12_OVERLAY, screen);

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
        );

    i=0;
	SDL_Event  event;

    //我们做了一些标准的文件打开，等等，然后写RGB数据。我们一次写一行文件。PPM文件只是一个以长字符串形式列出RGB信息的文件。
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            
            SDL_Rect rect;

            // Did we get a video frame?
            if(frameFinished) {
                SDL_LockYUVOverlay(bmp);

                AVPicture pict;
                pict.data[0] = bmp->pixels[0];
                pict.data[1] = bmp->pixels[2];
                pict.data[2] = bmp->pixels[1];

                pict.linesize[0] = bmp->pitches[0];
                pict.linesize[1] = bmp->pitches[2];
                pict.linesize[2] = bmp->pitches[1];

                // Convert the image from its native format into YUV format that SDL uses
				// 转换视频原始图形格式为YUV420格式，用于SDL显示
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                    pFrame->linesize, 0, pCodecCtx->height,
                    pict.data, pict.linesize);

                SDL_UnlockYUVOverlay(bmp);

                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;
                SDL_DisplayYUVOverlay(bmp, &rect);
            }
        }
            
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        SDL_PollEvent(&event);
        switch(event.type) 
        {
            case SDL_QUIT:
            SDL_Quit();
            exit(0);
            break;
            default:
            break;
        }
    }

    //现在，返回main()函数。一旦我们完成了视频流的阅读，我们只需要清理所有的东西:
    // Free the RGB image
    av_free(buffer);
    av_free(pFrameRGB);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;

}