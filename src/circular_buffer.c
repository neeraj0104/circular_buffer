/*
Copyright (c) 2016, Neeraj Prasad
 All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
- Neither the name of the 'incremental' nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE. */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>


typedef struct
{
    uint32_t r_idx;
    uint32_t w_idx;
    uint32_t circ_buffer_chunk_count;
    uint32_t circ_buffer_chunk_size;
    uint32_t total_size;

}x_circ_buffer_status_t;


typedef struct __circ_buffer_
{
    uint8_t *data_ptr;
    sem_t    semsignal;
    x_circ_buffer_status_t buffer;

}x_circ_buffer_t;


void* create_circ_buffer (uint32_t chunk_size, uint32_t chunk_count, uint32_t flags)
{
    uint32_t size = ((chunk_size + getpagesize() - 1) & ~(getpagesize() - 1));
    x_circ_buffer_t *px_circular_buffer = (x_circ_buffer_t *)malloc(sizeof(x_circ_buffer_t));

    if (px_circular_buffer == NULL)
        printf("Need to return\n");

    px_circular_buffer->data_ptr = malloc(size*chunk_count);
    sem_init(&(px_circular_buffer->semsignal), 0, 0);

    px_circular_buffer->buffer.w_idx  = 0;
    px_circular_buffer->buffer.r_idx  = 0;
    px_circular_buffer->buffer.circ_buffer_chunk_count = chunk_count;
    px_circular_buffer->buffer.circ_buffer_chunk_size = size;
    px_circular_buffer->buffer.total_size = (chunk_count * size);
LABEL_RETURN:
    return px_circular_buffer;
} 

void destroy_circ_buffer (void *p_desc)
{
    int32_t index = 0;
    x_circ_buffer_t *px_circular_buffer = (x_circ_buffer_t *)p_desc;

    if (px_circular_buffer == NULL)
    {
        printf("destroy_circ_buffer(): invalid params\n");
        goto LABEL_RETURN;
    }

    px_circular_buffer->buffer.w_idx  = 0;
    px_circular_buffer->buffer.r_idx  = 0;
    px_circular_buffer->buffer.circ_buffer_chunk_count = 0;
    px_circular_buffer->buffer.circ_buffer_chunk_size = 0;

    free(px_circular_buffer->data_ptr);
    px_circular_buffer->data_ptr = NULL;
    free(px_circular_buffer);
    px_circular_buffer = NULL;
LABEL_RETURN:
     return;
}

int32_t check_space_availability (void *p_desc)
{
    int32_t ret_val = 0;
    x_circ_buffer_t *px_circular_buffer = (x_circ_buffer_t *)p_desc;
 
    if (px_circular_buffer == NULL)
    {
        printf("check_space_availability(): invalid params\n");
        ret_val = -1;
    }
    else if ((px_circular_buffer->buffer.w_idx - px_circular_buffer->buffer.r_idx) < px_circular_buffer->buffer.circ_buffer_chunk_count)
    {
        ret_val = 1;
    }

    if (ret_val != 1)
       printf("check_space_availability : wpos:%u, rpos:%u, size:%u\n", 
                             px_circular_buffer->buffer.w_idx,
                             px_circular_buffer->buffer.r_idx,
                             px_circular_buffer->buffer.circ_buffer_chunk_count);
LABEL_RETURN:
    return ret_val;
}

int32_t post_chunk_to_circ_buffer (void *p_desc, void *ptr, uint32_t size)
{
    int32_t ret_val = -1;
    x_circ_buffer_t *px_circular_buffer = (x_circ_buffer_t *)p_desc;

    if (px_circular_buffer == NULL)
    {
        printf("post_chunk_to_circ_buffer(): invalid params\n");
        goto LABEL_RETURN;
    }

    if ((px_circular_buffer->buffer.w_idx - px_circular_buffer->buffer.r_idx) < px_circular_buffer->buffer.circ_buffer_chunk_count)
    {
        if (px_circular_buffer->buffer.w_idx > 50000)
        {
            while(px_circular_buffer->buffer.w_idx != px_circular_buffer->buffer.r_idx)
            {
                usleep(30*1000);
            }
            printf("post_chunk_to_circ_buffer(): resetting :%u\n", px_circular_buffer->buffer.w_idx);
            px_circular_buffer->buffer.w_idx = 0;
            px_circular_buffer->buffer.r_idx = 0;
        }
        memcpy (px_circular_buffer->data_ptr + ((px_circular_buffer->buffer.w_idx%px_circular_buffer->buffer.circ_buffer_chunk_count)*px_circular_buffer->buffer.circ_buffer_chunk_size), ptr, size);
        px_circular_buffer->buffer.w_idx++;
        sem_post(&(px_circular_buffer->semsignal));
        ret_val = 0;
    }
    else
    {
        printf("post_chunk_to_circ_buffer(): buffer not available\n");
        ret_val = -2;
    }  

LABEL_RETURN:
    return ret_val;
}

int32_t retrieve_chunk_from_circ_buffer (void *p_desc, void **ptr)
{
    int32_t ret_val = -1;
    x_circ_buffer_t *px_circular_buffer = (x_circ_buffer_t *)p_desc;

    if (px_circular_buffer == NULL)
    {
        printf("retrieve_chunk_from_circ_buffer(): invalid params\n");
        goto LABEL_RETURN;
    }

    ret_val = sem_wait(&(px_circular_buffer->semsignal));
    if (ret_val != 0)
    {
        ret_val = -2;
        goto LABEL_RETURN;
    }

    if ((px_circular_buffer->buffer.w_idx - px_circular_buffer->buffer.r_idx) >0)
    {
        *ptr = NULL;
        *ptr = (void *)(px_circular_buffer->data_ptr +((px_circular_buffer->buffer.w_idx%px_circular_buffer->buffer.circ_buffer_chunk_count)*px_circular_buffer->buffer.circ_buffer_chunk_size));
        ret_val = 0;
    }
    else
        ret_val = -3;
LABEL_RETURN:
    return ret_val;
}

int32_t return_chunk_to_circ_buffer (void *p_desc)
{
    int32_t ret_val = -1;
    x_circ_buffer_t *px_circular_buffer = (x_circ_buffer_t *)p_desc;

    if (px_circular_buffer == NULL)
    {
        printf("return_chunk_to_circ_buffer(): invalid params\n");
        goto LABEL_RETURN;
    }

    px_circular_buffer->buffer.r_idx++;
    //printf("return_chunk rpos:%d, wpos:%d\n", px_circular_buffer->buffer.r_idx, px_circular_buffer->buffer.w_idx);
    ret_val = 0;
LABEL_RETURN:
    return ret_val;
}



