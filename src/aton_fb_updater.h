/*
 Copyright (c) 2016,
 Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
 All rights reserved. See COPYING.txt for more details.
 */

#ifndef FBUpdater_h
#define FBUpdater_h

#include "aton_node.h"

// Our FrameBuffer updater thread
static void FBUpdater(unsigned index, unsigned nthreads, void* data)
{
    Aton* node = reinterpret_cast<Aton*>(data);
    double uiFrame, opFrame, prevFrame = 0;
    const int ms = 20;
    
    while (node->m_legit)
    {
        uiFrame = node->uiContext().frame();
        opFrame = node->outputContext().frame();
        
        if (node->m_multiframes && !node->m_framebuffers.empty())
        {
            if (uiFrame != prevFrame && uiFrame != opFrame)
            {
                node->flagForUpdate();
                prevFrame = uiFrame;
            }
        }
        else
            SleepMS(ms);
    }
}

#endif /* FBUpdater */
