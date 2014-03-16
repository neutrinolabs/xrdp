/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Encoder
 */

#include "xrdp_encoder.h"
#include "xrdp.h"
#include "thread_calls.h"
#include "fifo.h"

/**
 * Init encoder
 * 
 * @return 0 on success, -1 on failure
 *****************************************************************************/ 
 
int init_xrdp_encoder(struct xrdp_mm *mm)
{
    char buf[1024];
    
    g_writeln("xrdp_encoder.c: initing encoder");
        
    if (!mm)
        return -1;
        
    /* save mm so thread can access it */
    self = mm;
    
    /* setup required FIFOs */
    self->fifo_to_proc = fifo_create();
    self->fifo_processed = fifo_create();
    self->mutex = tc_mutex_create();
    
    /* setup wait objects for signalling */
    g_snprintf(buf, 1024, "xrdp_encoder_%8.8x", g_getpid());
    self->xrdp_encoder_event = g_create_wait_obj(buf);
    
    /* create thread to process messages */
    tc_thread_create(proc_enc_msg, 0);
    
    return 0;
}

/**
 * Deinit xrdp encoder
 *****************************************************************************/
 
void deinit_xrdp_encoder()
{
    XRDP_ENC_DATA *enc;
    FIFO          *fifo;
    
    g_writeln("xrdp_encoder.c: deiniting encoder");
    
    if (!self)
        return;

    /* destroy wait objects used for signalling */
    g_delete_wait_obj(self->xrdp_encoder_event);
    
    /* cleanup fifo_to_proc */
    fifo = self->fifo_to_proc;
    if (fifo)
    {
        while (!fifo_is_empty(fifo))
        {
            enc = fifo_remove_item(fifo);
            if (!enc)
                continue;
                
            g_free(enc->drects);
            g_free(enc->crects);
            g_free(enc);
        }
        
        fifo_delete(fifo);
    }
    
    /* cleanup fifo_processed */
    fifo = self->fifo_processed;
    if (fifo)
    {
        while (!fifo_is_empty(fifo))
        {
            enc = fifo_remove_item(fifo);
            if (!enc)
                continue;
                
            g_free(enc->drects);
            g_free(enc->crects);
            g_free(enc);
        }
        
        fifo_delete(fifo);
    }    
}

void *proc_enc_msg(void *arg)
{
    XRDP_ENC_DATA *enc;
    FIFO          *fifo_to_proc; 
    FIFO          *fifo_processed; 
    tbus           mutex;
    tbus           event;
    
    g_writeln("xrdp_encoder.c: process_enc_msg thread is running");
    
    fifo_to_proc = self->fifo_to_proc;
    fifo_processed = self->fifo_processed;
    mutex = self->mutex;
    event = self->xrdp_encoder_event;
    
    while (1)
    {
        /* get next msg */
        tc_mutex_lock(mutex);
        enc = fifo_remove_item(fifo_to_proc);
        tc_mutex_unlock(mutex);
        
        /* if no msg available, wait for signal */
        if (!enc)
        {
            g_writeln("###### JAY_TODO: waiting for msg....");
            g_tcp_can_recv(event, 1000);
            g_reset_wait_obj(event);
            continue;
        }
        
        /* process msg in enc obj */
        g_writeln("###### JAY_TODO: got msg....");
        /* JAY_TODO */
        
        /* done with msg */
        tc_mutex_lock(mutex);
        fifo_add_item(fifo_processed, enc);
        tc_mutex_unlock(mutex);
        
        /* signal completion */
        g_set_wait_obj(event);
        
    } /* end while (1) */
    
    return 0;
}
