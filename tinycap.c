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

    file = fopen(argv[1], "wb");
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

    return 0;
}

unsigned int capture_sample(FILE *file, unsigned int card, unsigned int device,
                            struct wav_header *header,unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    uint8_t *buffer;
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


    buffer = (uint8_t *)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(pcm);
        return 0;
    }

    printf("Capturing sample: %u ch, %u hz, %u bit\n", channels, rate,pcm_format_to_bits(format));
    int i=0,j=0;
    int ignore_size=0;
    int ignore_count=0;
    int voice_count = 0;
    uint8_t start_write=0;
    uint8_t file_temp_open=0;
    int frames_temp=0;
    int index=0;
    char *file_name = (char *)malloc(20);
    FILE *file_temp;
    int file_bytes_read=0;
    #define THRESHOLD_AUDIO 256
    #define COUNT_THRESHOLD 16
    #define SECTION_AUDIO   650

    printf("%d\n",size);
    while (capturing && !pcm_read(pcm, buffer, size)) 
    {

        for(j=0;j<=16-1;j++)
        {

            for(i=0;i<=size/16-2;i+=2)//whether is a audio period(1024*4*2)
            {
                //printf("%X\t",*(buffer+i)+(*(buffer+i+1)*256));
                if ((int16_t)(*(buffer+j * 1024 + i)|(*(buffer+j * 1024 + i + 1))<<8)<SECTION_AUDIO && (int16_t)(*(buffer +j * 1024+ i)|*(buffer +j * 1024+ i + 1)<<8)>-SECTION_AUDIO)ignore_size++;
                else ignore_size = 0;
                //printf("%d\t",(buffer+i)+*(buffer+i+1)*256);
            }

            if(ignore_size<=THRESHOLD_AUDIO&&start_write==0&& voice_count >= 4)
            {//if it's a audio period,open the file_temp
                start_write=1;
                file_temp_open=1;
                sprintf(file_name,"%d.wav",index);
                file_temp = fopen(file_name, "wb");
                if (!file_temp) 
                {
                    fprintf(stderr, "Unable to create temp_file '%s'\n", file_name);
                    return 1;
                }
            }

            if (ignore_size > THRESHOLD_AUDIO) 
            {
                voice_count = 0;
                ignore_count++;
            }
            else 
            {
                voice_count++;
                ignore_count = 0;
            }

            if(ignore_count>COUNT_THRESHOLD)start_write=0;

            if(start_write)
            {//,write to file_temp
                if (fwrite(buffer+j*1024, 1, size/16, file_temp) != size/16) 
                {
                    fprintf(stderr,"Error capturing sample\n");
                    break;
                }
                bytes_read += size/16;
            }  

            else if(file_temp_open)
            {//
                ignore_count=0;
                frames_temp=bytes_read/2;//pcm_bytes_to_frames(pcm, bytes_read);
                printf("Captured %d frames\n", frames_temp);
                  //   write header now all information is known 
                header->data_sz = frames_temp * header->block_align;
                header->riff_sz = header->data_sz + sizeof(struct wav_header) - 8;
                fseek(file_temp, 0, SEEK_SET);
                fwrite(header, sizeof(struct wav_header), 1, file_temp);
                printf("%d\n",index);
                index++;
                bytes_read=0;
                fclose(file_temp);
                file_temp_open=0;
            }
            
            ignore_size=0;
        }


        if (fwrite(buffer, 1, size, file) != size) 
        {
            fprintf(stderr,"Error capturing sample\n");
            break;
        }
        file_bytes_read+=size;
    }

    if(file_temp_open){
        frames_temp=pcm_bytes_to_frames(pcm, bytes_read);
        printf("Captured %d frames\n", frames_temp);
          //   write header now all information is known 
        header->data_sz = frames_temp * header->block_align;
        header->riff_sz = header->data_sz + sizeof(struct wav_header) - 8;
        fseek(file_temp, 0, SEEK_SET);
        fwrite(header, sizeof(struct wav_header), 1, file_temp);
        printf("%d\n",index);
        fclose(file_temp);
    }

    free(buffer);
    free(file_name);
    pcm_close(pcm);
    return file_bytes_read/2;
}

