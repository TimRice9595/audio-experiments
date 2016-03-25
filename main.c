//
//  main.c
//  audio_playground
//
//  Created by Timothy Rice on 2/14/16.
//  Copyright © 2016 Timothy Rice. All rights reserved.
//

#include <math.h>

#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>


#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
#define STD_SAMPLE_RATE 44100


//check if given sample format is accepted by given codec
static int check_sample_format(AVCodec* avco, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat* p = avco->sample_fmts;
    
    while(*p != AV_SAMPLE_FMT_NONE)
    {
        if(*p == sample_fmt)
        {
            return 1;
        }
        p++;
    }
    return 0;
}

//if no sample rates in codec, selects the standard rate of 44100.
//otherwise, highest suported sample rate is chosen
static int select_sample_rate(AVCodec* avco)
{
    
    const int* p;
    int best_sample_rate = 0;
    
    if(!(avco->supported_samplerates))
    {
        return STD_SAMPLE_RATE;
    }
    
    p = avco->supported_samplerates;
    
    while(*p)
    {
        best_sample_rate = FFMAX(*p, best_sample_rate);
        p++;
    }
    
    return best_sample_rate;
}

//select layout with the highest channel count
static int select_channel_layout(AVCodec* codec)
{
    const uint64_t* p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels = 0;
    
    if(!codec->channel_layouts)
    {
        return AV_CH_LAYOUT_STEREO;
    }
    
    p = codec->channel_layouts;
    while(*p)
    {
        int nb_channels = av_get_channel_layout_nb_channels(*p);
        
        if(nb_channels > best_nb_channels)
        {
            best_ch_layout = *p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

static int file_format_check(AVFormatContext* pFormatCtx, const char* song_path, FILE* infile)
{
    AVCodecContext* codecCtx = NULL;
    AVCodec* inputCodec = NULL;
    int audioStreamIndex, i = 0;
    
    //open audio file
    if(avformat_open_input(&pFormatCtx, song_path, NULL, NULL) != 0)
    {
        return -1;
    }
    
    //open binary file for reading
    infile = fopen(song_path, "rb");
    
    
    if(!infile)
    {
        fprintf(stderr, "Input file not opened!");
        return -1;
    }
    
    
    if(avformat_find_stream_info(pFormatCtx, NULL))
    {
        fprintf(stderr, "Stream info not found!");
        return -1;
    }
    
    //dumps info about format context to standard error
    av_dump_format(pFormatCtx, 0, /*argv[1]*/0, 0);
    
    //find audio stream
    for(i = 0; i < pFormatCtx->nb_streams; i++){
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioStreamIndex = i;
            printf("Audio Stream Found!\n");
            break;
        }
    }
    
    if(audioStreamIndex == -1)
    {
        return -1;
    }
    
    //set codec context pointer to codec context of audio stream
    codecCtx = pFormatCtx->streams[audioStreamIndex]->codec;
    //find decodeder for audio stream
    inputCodec = avcodec_find_decoder(codecCtx->codec_id);
    if (inputCodec == NULL)
    {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
    
    //open codec
    if(avcodec_open2(codecCtx, inputCodec, NULL) < 0)
    {
        fprintf(stderr, "Codec could not be opened!");
        return -1;
    }
    
    
    return audioStreamIndex;
}


int main(int argc, const char * argv[]) {
    
    //refister all available file formats and codecs
    av_register_all();
    
    FILE *infile, *outfile;
    int i, len, ret, audioStreamIndex = -1;
    const char* song_path = "/Users/timothyrice/Dropbox/audio-experimentation/audio_playground/audio_playground/Monsters.mp3";
    const char* new_format_path = "/Users/timothyrice/Dropbox/audio-experimentation/audio_playground/audio_playground/new_audio.mp3";
    
    
    AVFormatContext* inputFormat = avformat_alloc_context();
    AVFormatContext* outputFormat = avformat_alloc_context();
    
    AVCodecContext* decodeCodecCtx;
    AVPacket decodingPacket;
    AVFrame* decodedFrame = NULL;
    
    AVCodec* encodeCodec = NULL;
    AVCodecContext* encodeCodecCtx = NULL;
    AVPacket encodingPacket;
    
    
    audioStreamIndex = file_format_check(inputFormat, song_path, infile);
    
    if(audioStreamIndex < 0)
    {
        return -1;
    }
    
    av_init_packet(&decodingPacket);
    
    //allocate frame
    if(!decodedFrame)
    {
        if(!(decodedFrame = av_frame_alloc()))
        {
            fprintf(stderr, "Could not allocate audio frame\n");
            return -1;
        }
        
    }
    
    //set decoding codec context to audio stream index's codec
    decodeCodecCtx = inputFormat->streams[audioStreamIndex]->codec;
    
    //set up encoding audio codec
    encodeCodec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if(!encodeCodec)
    {
        fprintf(stderr, "Could not find encoder codec!\n");
        return -1;
    }
    encodeCodecCtx = avcodec_alloc_context3(encodeCodec);
    
    encodeCodecCtx->bit_rate = decodeCodecCtx->bit_rate;
    encodeCodecCtx->sample_fmt = decodeCodecCtx->sample_fmt;
    
    if(!check_sample_format(encodeCodec, encodeCodecCtx->sample_fmt))
    {
        fprintf(stderr, "Sample format %s not supported by encoding codec!\n", av_get_sample_fmt_name(encodeCodecCtx->sample_fmt));
        return -1;
    }
    
    encodeCodecCtx->sample_rate = select_sample_rate(encodeCodec);
    encodeCodecCtx->channel_layout = select_channel_layout(encodeCodec);
    encodeCodecCtx->channels = av_get_channel_layout_nb_channels(encodeCodecCtx->channel_layout);
    
    if(avcodec_open2(encodeCodecCtx, encodeCodec, 0) < 0)
    {
        fprintf(stderr, "Could not open encoder codec\n");
        return -1;
    }
    
    outfile = fopen(new_format_path, "wb");
    
    if(!outfile)
    {
        fprintf(stderr, "Output file not opened!\n");
        return -1;
    }
    
    //printf("Outside function format-duration = %lld\n", inputFormat->duration);
    int c = 0, sum = 0;// max = 0;
    while (av_read_frame(inputFormat, &decodingPacket) == 0)
    {
        //CHECK LATER:# of samples decoded = # encoded?
        //PLANAR????
        
        int ch, got_frame, got_packet = 0;
        int j = 0;
        //printf("Frame #%d\n", ++c);
        
        //make sure stream indexes match, rarely necessary but can prevent a crash
        if(decodingPacket.stream_index == audioStreamIndex)
        {
            
            int bl = 0; //temporary to see performance of while loop
            
            while (decodingPacket.size > 0) {
                got_frame = 0;
                
                if(bl++ > 1)
                {
                    printf("while loop repeat!!!!\n\n\n");
                }
                
                //decodes next frame from stream inside decodingPacket and stores it in decodedFrame
                //len < 0 if the decoding didn't work
                len = avcodec_decode_audio4(decodeCodecCtx, decodedFrame, &got_frame, &decodingPacket);

                

                if(got_frame && len >= 0)
                {
                
  
                    int data_size = av_get_bytes_per_sample(decodeCodecCtx->sample_fmt);
            
                    if(data_size < 0)
                    {
                        fprintf(stderr, "Failed to calculate data size\n");
                        return -1;
                    }
                    
                    
                    ret = avcodec_encode_audio2(encodeCodecCtx, &encodingPacket, decodedFrame, &got_packet);
                    
                    if(ret < 0)
                    {
                        fprintf(stderr, "Error encoding raw audio frame!\n");
                        return -1;
                    }
                    
                    if(got_packet)
                    {
                        
                        fwrite(encodingPacket.data, 1, encodingPacket.size, outfile);
                        av_free_packet(&encodingPacket);
                        
                        if(encodeCodecCtx->codec->capabilities & CODEC_CAP_DELAY)
                        {
                            av_init_packet(&encodingPacket);
                            
                            got_frame = 0;
                            while(avcodec_encode_audio2(encodeCodecCtx, &encodingPacket, NULL, &got_packet) >= 0 && got_frame)
                            {
                                fwrite(encodingPacket.data, 1, encodingPacket.size, outfile);
                                av_free_packet(&encodingPacket);
                            }
                        }
                    }
                    

                    /*if(decodeCodecCtx->codec->capabilities & CODEC_CAP_DELAY)
                    {
                        av_init_packet(&decodingPacket);
                        
                        got_frame = 0;
                        while(avcodec_decode_audio4(decodeCodecCtx, decodedFrame, &got_frame, &decodingPacket) >= 0 && got_frame)
                        {
                            printf("FUCK!\n");
                        }
                    }*/
                }
                else
                {
                    fprintf(stderr, "Error while decoding\n");
                    return -1;
                    
                }
                
            decodingPacket.size -= len;
            decodingPacket.data += len;
            decodingPacket.dts = AV_NOPTS_VALUE;
            decodingPacket.pts = AV_NOPTS_VALUE;
                
            }
            
            av_free_packet(&decodingPacket);
        }
            
    }
    
    
    //open audio file
    if(avformat_open_input(&outputFormat, new_format_path, NULL, NULL) != 0)
    {
        return -1;
    }
    
    //open binary file for reading
    outfile = fopen(new_format_path, "rb");
    
    
    if(!outfile)
    {
        fprintf(stderr, "Input file not opened!");
        return -1;
    }
    
    
    if(avformat_find_stream_info(outputFormat, NULL))
    {
        fprintf(stderr, "Stream info not found!");
        return -1;
    }
    
    //dumps info about format context to standard error
    av_dump_format(outputFormat, 0, /*argv[1]*/0, 0);
    
    fclose(infile);
    fclose(outfile);
        
    avcodec_close(decodeCodecCtx);
    av_free(decodeCodecCtx);
    av_frame_free(&decodedFrame);
        
    
    
    
    return 0;
}
