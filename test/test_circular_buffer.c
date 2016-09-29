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

#include "circular_buffer.h"

typedef struct
{
    uint32_t reader_id;
    void *p_desc;
    
}multiread_x;


void *do_read_cb_messages(void *args)
{
    multiread_x *param = (multiread_x *)args;
    int counter = 0, ret = 0;
    char *buffer=NULL;

    while (counter < 5000000)
    {
        ret = retrieve_chunk_from_circ_buffer (param->p_desc, &buffer);
        if (ret < 0)
        {
            printf("facelift_pool_retrieve_chunk[%d]: some issue..[%d]\n", param->reader_id, ret);
            break;
        }
        else
        {
          //  printf("facelift_pool_retrieve_chunk [%d]: %s\n", param->reader_id, buffer);
            ret = return_chunk_to_circ_buffer(param->p_desc);
            if (ret < 0)
            {
                printf("facelift_pool_retrieve_chunk [%d]: some issue..[%d]\n", param->reader_id, ret);
                break;
            }

        }
        counter++;
    }
    printf("Coming out of read thread\n");
    return NULL;
}

int main()
{
    void *p_desc = NULL;
    int counter = 0, ret = 0;
    char buffer [4*1024*1000] = {0};
    pthread_t pid1, pid2;
    multiread_x facelift_msg={0};

    p_desc = create_circ_buffer (4*1024*1000, 4, 0);

    if (p_desc)
        printf("Facelift_pool_sender connection established\n");
    else
        goto LABEL_RETURN;


    facelift_msg.reader_id = 1;
    facelift_msg.p_desc = p_desc;

    pthread_create(&pid1, NULL, do_read_cb_messages, &facelift_msg);
    pthread_detach(pid1);

    while (counter < 5000000)
    {
        snprintf(buffer, sizeof(buffer), "MSG [%2d]: from server",counter);
        if (check_space_availability(p_desc))
        {
            ret = post_chunk_to_circ_buffer (p_desc, buffer, 4*1024*1000);
            if (ret < 0)
            {
                printf("Facelift_pool_sender: some issue..at [%u]; ret_val:%d\n", counter,ret);
                break;
            }
           // printf("Facelift_pool_sender: sent_count: %d\n", counter);
        }
        else
        {
            printf("Facelift_pool_sender: buffer underflow..at [%u]; ret_val:%d\n", counter,ret);
            sleep(1);
            continue;
        }
        counter++;
        usleep(30);
    }
    printf("Write server done..\n");
    sleep(5);
    destroy_circ_buffer (p_desc);

LABEL_RETURN:
    printf("Facelift pool sender exiting\n");
    return 0;
}

