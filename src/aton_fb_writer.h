/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FBWriter_h
#define FBWriter_h

#include "boost/format.hpp"

#include "aton_node.h"

long getFBIndex(Aton* node, const long long& index)
{
    long fb_index = 0;
    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
    
    if (!fbs.empty())
    {
        std::vector<FrameBuffer>::iterator it;
        for(it = fbs.begin(); it != fbs.end(); ++it)
        {
            if (it->getSessionIndex() == index)
                fb_index = it - fbs.begin();
            else
                fb_index = fbs.size();
        }
    }
    return fb_index;
}

FrameBuffer& addFrameBuffer(Aton* node)
{
    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
    
    FrameBuffer fb;
    fbs.push_back(fb);
    node->m_outputKnobChanged = Aton::item_added;
    return fbs.back();
}

// Our RenderBuffer writer thread
static void FBWriter(unsigned index, unsigned nthreads, void* data)
{
    bool killThread = false;
    std::vector<std::string> active_aovs;

    Aton* node = reinterpret_cast<Aton*> (data);

    while (!killThread)
    {
        // Accept incoming connections!
        node->m_server.accept();

        // Our incoming data object
        int dataType = 0;
        
        // Session Index
        long long session_idx = 0;
        
        // For progress percentage
        long long progress, regionArea = 0;
        
        // Time to reset per every IPR iteration
        static int _active_time, delta_time = 0;
        
        // Loop over incoming data
        while (dataType != 2 || dataType != 9)
        {
            // Listen for some data
            try
            {
                dataType = node->m_server.listenType();
                WriteGuard lock(node->m_mutex);
                node->m_running = true;
            }
            catch( ... )
            {
                WriteGuard lock(node->m_mutex);
                node->m_running = false;
                node->flagForUpdate();
                break;
            }
            
            // Handle the data we received
            switch (dataType)
            {
                case 0: // Open a new image
                {
                    // Get Data Header
                    DataHeader dh = node->m_server.listenHeader();

                    const long long& _index = dh.index();
                    const int& _xres = dh.xres();
                    const int& _yres = dh.yres();
                    const float& _pix_aspect = dh.pixel_aspect();
                    const long long& _area = dh.rArea();
                    const int& _version = dh.version();
                    const double& _frame = static_cast<double>(dh.currentFrame());
                    const float& _fov = dh.camFov();
                    const Matrix4& _matrix = Matrix4(&dh.camMatrix()[0]);
                    const std::vector<int> _samples = dh.samples();
                    const char* _output_name = dh.outputName();

                    // Output String
                    std::string _output_string = (boost::format("%s_%d_%s")%_output_name%_frame
                                                                           %node->getDateTime()).str();
                    // Get image area to calculate the progress
                    regionArea = _area;

                    // Get delta time per IPR iteration
                    delta_time = _active_time;

                    // Get Current Session Index
                    session_idx = _index;

                    bool& multiframe = node->m_multiframes;
                    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
                
                    // Get FrameBuffer Index
                    long fb_index = getFBIndex(node, _index);
                    
                    if (multiframe)
                    {
                        if (!fbs.empty())
                        {
                           if (fb_index == fbs.size())
                               fb_index --;
                            
                            FrameBuffer& fb = fbs[fb_index];
                            
                            if (!fb.frameExists(_frame))
                            {
                                WriteGuard lock(node->m_mutex);
                                fb.addFrame(_index, _output_string, _frame, _xres, _yres, _pix_aspect);
                            }
                            else
                            {
                                WriteGuard lock(node->m_mutex);
                                fb.setSessionIndex(_index);
                                fb.setOutputName(_output_string);
                                fb.setCurrentFrame(_frame);
                            }
                        }
                    }
                    else
                    {
                        if (!fbs.empty())
                        {
                            if (fb_index == fbs.size())
                            {
                                WriteGuard lock(node->m_mutex);
                                FrameBuffer& fb = addFrameBuffer(node);
                                fb.addFrame(_index, _output_string, _frame, _xres, _yres, _pix_aspect);
                            }
                        }
                    }
                    
                    if (fbs.empty())
                    {
                        WriteGuard lock(node->m_mutex);
                        FrameBuffer& fb = addFrameBuffer(node);
                        fb.addFrame(_index, _output_string, _frame, _xres, _yres, _pix_aspect);
                    }
                    
                    // Get current RenderBuffer
                    RenderBuffer& rb = fbs[fb_index].getFrame(_frame);
                    
                    // Reset Frame and Buffers if changed
                    if (!rb.empty() && !active_aovs.empty())
                    {
                        if (rb.isFrameChanged(_frame))
                        {
                            WriteGuard lock(node->m_mutex);
                            rb.setFrame(_frame);
                        }
                        if(rb.isAovsChanged(active_aovs))
                        {
                            WriteGuard lock(node->m_mutex);
                            rb.resize(1);
                            rb.ready(false);
                            node->resetChannels(node->m_channels);
                        }
                    }
                    
                    // Set Camera
                    if (rb.isCameraChanged(_fov, _matrix))
                    {
                        WriteGuard lock(node->m_mutex);
                        rb.setCamera(_fov, _matrix);
                        node->setCameraKnobs(rb.getCameraFov(),
                                             rb.getCameraMatrix());
                    }
                    
                    // Set Version
                    if (rb.getVersionInt() != _version)
                    {
                        WriteGuard lock(node->m_mutex);
                        rb.setVersion(_version);
                    }
                    
                    // Set Samples
                    if (rb.getSamplesInt() != _samples)
                    {
                        WriteGuard lock(node->m_mutex);
                        rb.setSamples(_samples);
                    }
                    
                    // Reset active AOVs
                    if(!active_aovs.empty())
                        active_aovs.clear();
                    
                    // Set Frame on Timeline
                    if (_frame != node->outputContext().frame())
                        node->setCurrentFrame(_frame);
                    break;
                }
                case 1: // Write image data
                {
                    // Get Data Pixels
                    DataPixels dp = node->m_server.listenPixels();
                    
                    const int& _xres = dp.xres();
                    const int& _yres = dp.yres();
                    const char* _aov_name = dp.aovName();

                    // Get Render Buffer
                    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
                    long fb_index = getFBIndex(node, session_idx);
                    
                    if (fb_index == fbs.size())
                        fb_index-- ;
                    
                    RenderBuffer& rb = fbs[fb_index].currentFrame();

                    if(rb.isResolutionChanged(_xres, _yres))
                    {
                        WriteGuard lock(node->m_mutex);
                        rb.setResolution(_xres, _yres);
                    }

                    // Get active aov names
                    if(std::find(active_aovs.begin(),
                                 active_aovs.end(),
                                 _aov_name) == active_aovs.end())
                    {
                        if (node->m_enable_aovs || active_aovs.empty())
                            active_aovs.push_back(_aov_name);
                        else if (active_aovs.size() > 1)
                            active_aovs.resize(1);
                    }
                    
                    // Skip non RGBA buckets if AOVs are disabled
                    if (node->m_enable_aovs || active_aovs[0] == _aov_name)
                    {
                        // Get Data Pixels
                        const int& _x = dp.bucket_xo();
                        const int& _y = dp.bucket_yo();
                        const int& _width = dp.bucket_size_x();
                        const int& _height = dp.bucket_size_y();
                        const int& _spp = dp.spp();
                        const long long& _ram = dp.ram();
                        const int& _time = dp.time();

                        // Set active time
                        _active_time = _time;

                        // Get framebuffer width and height
                        const int& w = rb.getWidth();
                        const int& h = rb.getHeight();

                        // Adding buffer
                        node->m_mutex.writeLock();
                        if(!rb.aovExists(_aov_name) && (node->m_enable_aovs || rb.empty()))
                            rb.addBuffer(_aov_name, _spp);
                        else
                            rb.ready(true);

                        // Get buffer index
                        const int b = rb.aovIndex(_aov_name);

                        // Writing to buffer
                        int x, y, c, xpos, ypos, offset;
                        for (x = 0; x < _width; ++x)
                        {
                            for (y = 0; y < _height; ++y)
                            {
                                offset = (_width * y * _spp) + (x * _spp);
                                for (c = 0; c < _spp; ++c)
                                {
                                    xpos = x + _x;
                                    ypos = h - (y + _y + 1);
                                    const float& _pix = dp.pixel(offset + c);
                                    rb.setBufferPix(b, xpos, ypos, _spp, c, _pix);
                                }
                            }
                        }
                        node->m_mutex.unlock();

                        // Update only on first aov
                        if(!node->m_capturing && rb.isFirstBufferName(_aov_name))
                        {
                            // Calculate the progress percentage
                            regionArea -= _width * _height;
                            progress = 100 - (regionArea * 100) / (w * h);
                            
                            // Set status parameters
                            node->m_mutex.writeLock();
                            rb.setProgress(progress);
                            rb.setRAM(_ram);
                            rb.setTime(_time, delta_time);
                            node->m_mutex.unlock();

                            // Update the image
                            const Box box = Box(_x, h - _y - _width, _x + _height, h - _y);
                            node->flagForUpdate(box);
                        }
                    }
                    dp.free();
                    break;
                }
                case 2: // Close image
                {
                    break;
                }
                case 9: // When the parent process want to kill the listening thread
                {
                    killThread = true;
                    break;
                }
            }
        }
    }
}

#endif /* FBWriter_h */
