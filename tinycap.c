/* tinycap.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#include "asoundlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

int capturing = 1;

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            struct wav_header *header,unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count);

void sigint_handler(int sig)
{
    capturing = 0;
}

int main(int argc, char **argv)
{
    FILE *file;
    struct wav_header header;
    unsigned int card = 0;
    unsigned int device = 0;
    unsigned int channels = 1;
    unsigned int rate = 44100;
    unsigned int bits = 16;
    unsigned int frames;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;
    enum pcm_format format;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.wav [-D card] [-d device] [-c channels] "
                "[-r rate] [-b bits] [-p period_size] [-n n_periods]\n", argv[0]);
        return 1;
    }

    char * file_in_name=malloc(20);
    file_in_name=argv[1];
    file = fopen(file_in_name, "wb+");
    if (!file) {
        fprintf(stderr, "Unable to create file '%s'\n", argv[1]);
        return 1;
    }

    /* parse command line arguments */
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            if (*argv)
                device = atoi(*argv);
        } else if (strcmp(*argv, "-c") == 0) {
            argv++;
            if (*argv)
                channels = atoi(*argv);
        } else if (strcmp(*argv, "-r") == 0) {
            argv++;
            if (*argv)
                rate = atoi(*argv);
        } else if (strcmp(*argv, "-b") == 0) {
            argv++;
            if (*argv)
                bits = atoi(*argv);
        } else if (strcmp(*argv, "-D") == 0) {
            argv++;
            if (*argv)
                card = atoi(*argv);
        } else if (strcmp(*argv, "-p") == 0) {
            argv++;
            if (*argv)
                period_size = atoi(*argv);
        } else if (strcmp(*argv, "-n") == 0) {
            argv++;
            if (*argv)
                period_count = atoi(*argv);
        }
        if (*argv)
            argv++;
    }

    header.riff_id = ID_RIFF;
    header.riff_sz = 0;
    header.riff_fmt = ID_WAVE;
    header.fmt_id = ID_FMT;
    header.fmt_sz = 16;
    header.audio_format = FORMAT_PCM;
    header.num_channels = channels;
    header.sample_rate = rate;

    switch (bits) {
    case 32:
        format = PCM_FORMAT_S32_LE;
        break;
    case 24:
        format = PCM_FORMAT_S24_LE;
        break;
    case 16:
        format = PCM_FORMAT_S16_LE;
        break;
    default:
        fprintf(stderr, "%d bits is not supported.\n", bits);
        return 1;
    }

    header.bits_per_sample = pcm_format_to_bits(format);
    header.byte_rate = (header.bits_per_sample / 8) * header.num_channels * header.sample_rate;
    header.block_align = channels * (header.bits_per_sample / 8);
    header.data_id = ID_DATA;

    /* leave enough room for header */
    fseek(file, sizeof(struct wav_header), SEEK_SET);

    /* install signal handler and begin capturing */
    signal(SIGINT, sigint_handler);
    frames = capture_sample(file, card, device, &header,header.num_channels,
                            header.sample_rate, format,
                            period_size, period_count);
    
    printf("Captured %d frames\n", frames);  

            /* write header now all information is known */
    header.data_sz = frames * header.block_align;
    header.riff_sz = header.data_sz + sizeof(header) - 8;
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(struct wav_header), 1, file);
    fclose(file);


#define COUNT 1
    uint8_t *buf;
    int ignore_size = 0;
    int ignore_count = 0;
    int voice_count = 0;
    uint8_t start_write = 0;
    uint8_t file_temp_open = 0;
    int frames_temp = 0;
    int index = 0;
    char *file_name = (char *)malloc(20);
    FILE *file_temp;
    FILE *file_in;
#define THRESHOLD_AUDIO 256
#define COUNT_THRESHOLD 16
    int bytes_read = 0;
    int flag = 0;

    buf = (uint8_t *)malloc(sizeof(struct wav_header)*COUNT);

    if (buf == NULL)fprintf(stderr, "malloc fail\n");

    file_in = fopen(file_in_name, "rb");

    if (!file_in)
        fprintf(stderr, "Unable to open file\n");
    else fprintf(stderr, "Open file success\n");

    fseek(file_in, 0, SEEK_SET);

    flag = fread(buf, sizeof(struct wav_header), COUNT, file_in);
    header.riff_id = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.riff_sz = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.riff_fmt = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.fmt_id= *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.fmt_sz = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.audio_format = *buf | *(buf + 1) << 8; buf += 2;
    header.num_channels = *buf | *(buf + 1) << 8; buf += 2;
    header.sample_rate = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.byte_rate = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.block_align = *buf | *(buf + 1) << 8; buf += 2;
    header.bits_per_sample = *buf | *(buf + 1) << 8; buf += 2;
    header.data_id= *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf += 4;
    header.data_sz = *buf | *(buf + 1) << 8 | *(buf + 2) << 16 | *(buf + 3) << 24; buf = buf + 4 - sizeof(struct wav_header);

    // printf("riff_id:%X\n",header.riff_id);
    // printf("riff_sz:%X\n", header.riff_sz);
    // printf("riff_fmt:%X\n", header.riff_fmt);
    // printf("fmt_id:%X\n", header.fmt_id);
    // printf("fmt_sz:%X\n", header.fmt_sz);
    // printf("audio_format:%X\n", header.audio_format);
    // printf("num_channels:%X\n", header.num_channels);
    // printf("sample_rate:%u\n", header.sample_rate);
    // printf("byte_rate:%u\n", header.byte_rate);
    // printf("block_align:%X\n", header.block_align);
    // printf("bits_per_sample:%X\n", header.bits_per_sample);
    // printf("data_id:%X\n", header.data_id);
    // printf("data_sz:%d\n", header.data_sz);
    // printf("header.size:%d\n", sizeof(struct wav_header));

    free(buf);

            
    //         fclose(file);


    // file = fopen(argv[1], "rb+");
    // if (!file) {
    //     fprintf(stderr, "Unable to create file111 '%s'\n", argv[1]);
    //     return 1;
    // }

            #define PERIOD 1024

    buf = (uint8_t *)malloc(PERIOD);
    if (buf == NULL)fprintf(stderr, "malloc fail\n");
    int j = header.data_sz;
 while (j) {
        flag=fread(buf, PERIOD, 1, file_in);
        
        if (!flag)
            fprintf(stderr, "Unable to read file\n");
        if (ferror(file_in))
            fprintf(stderr, "File read error\n");
        if (feof(file_in))
            fprintf(stderr, "File reach end\n");

        //for (int i = 0; i <= PERIOD - 1; i++)printf("%X\t", *(buf + i));

        int i=0;
        for (i = 0; i<PERIOD - 2; i+=2) {
            if ((int16_t)(*(buf + i)|(*(buf + i + 1))<<8)<650 && (int16_t)(*(buf + i)|*(buf + i + 1)<<8)>-650)ignore_size++;
            else ignore_size = 0;
            //printf("%d\t", (int16_t)(*(buf + i) | (*(buf + i + 1)) << 8));
        }


        
        if (ignore_size <= THRESHOLD_AUDIO&&start_write == 0&&voice_count>= 4) {//if it's a audio period,open the file_temp
            start_write = 1;
            file_temp_open = 1;
            sprintf(file_name, "%d.wav", index);
            file_temp = fopen(file_name, "wb");
            if (!file_temp) {
                fprintf(stderr, "Unable to create temp_file '%s'\n", file_name);
                return 1;
            }
        }

        
        if (ignore_size > THRESHOLD_AUDIO) {
            voice_count = 0;
            ignore_count++;
        }
        else {
            voice_count++;
            ignore_count = 0;
        }

        if (ignore_count>COUNT_THRESHOLD)start_write = 0;

        if (start_write) {//,write to file_temp
            if (fwrite(buf, 1, PERIOD, file_temp) != PERIOD) {
                fprintf(stderr, "Error capturing sample\n");
                break;
            }
            bytes_read += PERIOD;
        }

        else if (file_temp_open) {//
            ignore_count = 0;
            frames_temp = bytes_read / 2;
            printf("Captured %d frames\n", frames_temp);
            //   write header now all information is known 
            header.data_sz = frames_temp * header.block_align;
            header.riff_sz = header.data_sz + sizeof(struct wav_header) - 8;
            fseek(file_temp, 0, SEEK_SET);
            fwrite(&header, sizeof(struct wav_header), 1, file_temp);
            printf("%d\n", index);
            index++;
            bytes_read = 0;
            fclose(file_temp);
            file_temp_open = 0;
        }

        ignore_size = 0;
        j -= PERIOD;
     }

     free(file_name);
     fclose(file_in);

    return 0;
}

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            struct wav_header *header,unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    char *buffer;
    unsigned int size;
    unsigned int bytes_read = 0;

    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    config.format = format;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    pcm = pcm_open(card, device, PCM_IN, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        fprintf(stderr, "Unable to open PCM device (%s)\n",
                pcm_get_error(pcm));
        return 0;
    }

    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(pcm);
        return 0;
    }

    printf("Capturing sample: %u ch, %u hz, %u bit\n", channels, rate,
           pcm_format_to_bits(format));


            
    while (capturing && !pcm_read(pcm, buffer, size)) {
        // for(i=0;i<size-2;i++)//whether is a audio period
        // {
        //     //printf("%X\t",*(buffer+i)+(*(buffer+i+1)*256));
        //     if((int16_t)(*(buffer+i)+*(buffer+i+1)*256)<650&&(int16_t)(*(buffer+i)+*(buffer+i+1)*256)>-650)ignore_size++;
        //     else ignore_size=0;
        //     //printf("%d\t",(buffer+i)+*(buffer+i+1)*256);
        // }

        // if(ignore_size<=THRESHOLD_AUDIO&&start_write==0){//if it's a audio period,open the file_temp
        //     start_write=1;
        //     file_temp_open=1;
        //     sprintf(file_name,"%d.wav",index);
        //     file_temp = fopen(file_name, "wb");
        //     if (!file_temp) {
        //         fprintf(stderr, "Unable to create temp_file '%s'\n", file_name);
        //         return 1;
        //     }
        // }

        // if(ignore_size>THRESHOLD_AUDIO)ignore_count++;
        // else ignore_count=0;

        // if(ignore_count>COUNT_THRESHOLD)start_write=0;

        // if(start_write){//,write to file_temp
            if (fwrite(buffer, 1, size, file) != size) {
            fprintf(stderr,"Error capturing sample\n");
            break;
        }
            bytes_read += size;
        // }

        // else if(file_temp_open){//
        // ignore_count=0;
        // frames_temp=pcm_bytes_to_frames(pcm, bytes_read);
        // printf("Captured %d frames\n", frames_temp);
        //   //   write header now all information is known 
        // header->data_sz = frames_temp * header->block_align;
        // header->riff_sz = header->data_sz + sizeof(struct wav_header) - 8;
        // fseek(file_temp, 0, SEEK_SET);
        // fwrite(header, sizeof(struct wav_header), 1, file_temp);
        // printf("%d\n",index);
        // index++;
        // bytes_read=0;
        // fclose(file_temp);
        // file_temp_open=0;
        // }
        
        // ignore_size=0;
    }
        // frames_temp=pcm_bytes_to_frames(pcm, bytes_read);
        // printf("Captured %d frames\n", frames_temp);
        //   //   write header now all information is known 
        // header->data_sz = frames_temp * header->block_align;
        // header->riff_sz = header->data_sz + sizeof(struct wav_header) - 8;
        // fseek(file_temp, 0, SEEK_SET);
        // fwrite(header, sizeof(struct wav_header), 1, file_temp);
        // printf("%d\n",index);
        // fclose(file_temp);

    //     printf("333\n");
    free(buffer);

    pcm_close(pcm);
    return pcm_bytes_to_frames(pcm, bytes_read);
}

