/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/


#ifndef FBUpdater_h
#define FBUpdater_h

#include "aton_node.h"

// Our FrameBuffer updater thread
static void FBUpdater(unsigned index, unsigned nthreads, void* data)
{
    Aton* node = reinterpret_cast<Aton*>(data);
    double frame, prevFrame = 0;
    const int ms = 20;
    
    while (node->m_legit)
    {
        frame = node->outputContext().frame();
        
        if (node->m_multiframes &&
            !node->m_framebuffers.empty() && frame != prevFrame)
        {
            node->flagForUpdate();
            prevFrame = frame;
        }
        else
            SleepMS(ms);
    }
}

#endif /* FBUpdater */
