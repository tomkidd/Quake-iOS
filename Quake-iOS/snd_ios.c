/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"
#include <AudioToolbox/AudioQueue.h>
#include <pthread.h>

AudioQueueRef snd_audioqueue = NULL;

pthread_mutex_t snd_lock;

volatile int snd_current_sample_pos = 0;

void SNDDMA_Callback(void *userdata, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
    pthread_mutex_lock(&snd_lock);
    
    if (snd_audioqueue == nil)
    {
        return;
    }

    memcpy(buffer->mAudioData, shm->buffer + (snd_current_sample_pos << 1), shm->samples >> 2);

    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
    
    snd_current_sample_pos += (shm->samples >> 3);
    
    if(snd_current_sample_pos >= shm->samples)
    {
        snd_current_sample_pos = 0;
    }
    
    pthread_mutex_unlock(&snd_lock);
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

qboolean SNDDMA_Init(void)
{
    pthread_mutex_init(&snd_lock, NULL);
    
    pthread_mutex_lock(&snd_lock);

    shm = (void *) Hunk_AllocName(sizeof(*shm), "shm");
    shm->splitbuffer = 0;
    shm->samplebits = 16;
    shm->speed = 22050;
    shm->channels = 2;
    shm->samples = 32768;
    shm->samplepos = 0;
    shm->soundalive = true;
    shm->gamealive = true;
    shm->submission_chunk = (shm->samples >> 3);
    shm->buffer = Hunk_AllocName(shm->samples * (shm->samplebits >> 3) * shm->channels, "shmbuf");
    
    AudioStreamBasicDescription format;
    
    format.mSampleRate = shm->speed;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = shm->samplebits;
    format.mChannelsPerFrame = shm->channels;
    format.mBytesPerFrame = shm->channels * (shm->samplebits >> 3);
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;
    format.mReserved = 0;
    
    OSStatus status = AudioQueueNewOutput(&format, SNDDMA_Callback, NULL, NULL, 0, 0, &snd_audioqueue);
    
    if (status != 0)
    {
        return false;
    }

    AudioQueueBufferRef buffer;
    
    status = AudioQueueAllocateBuffer(snd_audioqueue, shm->samples >> 2, &buffer);
    
    if (status != 0)
    {
        return false;
    }
    
    buffer->mAudioDataByteSize = shm->samples >> 2;
    
    status = AudioQueueEnqueueBuffer(snd_audioqueue, buffer, 0, NULL);

    if (status != 0)
    {
        AudioQueueFreeBuffer(snd_audioqueue, buffer);

        return false;
    }
    
    status = AudioQueueAllocateBuffer(snd_audioqueue, shm->samples >> 2, &buffer);
    
    if (status != 0)
    {
        return false;
    }
    
    buffer->mAudioDataByteSize = shm->samples >> 2;
    
    AudioQueueEnqueueBuffer(snd_audioqueue, buffer, 0, NULL);

    if (status != 0)
    {
        AudioQueueFreeBuffer(snd_audioqueue, buffer);
        
        return false;
    }
    
    status = AudioQueueStart(snd_audioqueue, NULL);
    
    if (status != 0)
    {
        return false;
    }
    
    snd_current_sample_pos = shm->samples >> 1;
    
    pthread_mutex_unlock(&snd_lock);
    
    return true;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
    shm->samplepos = snd_current_sample_pos;
    
    return shm->samplepos;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
    pthread_mutex_lock(&snd_lock);
    
    if (snd_audioqueue != NULL)
    {
        AudioQueueStop(snd_audioqueue, false);

        AudioQueueDispose(snd_audioqueue, false);
    }
    
    snd_audioqueue = NULL;
    
    pthread_mutex_unlock(&snd_lock);
}

