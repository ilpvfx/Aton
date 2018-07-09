/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FBWriter_h
#define FBWriter_h

#include "aton_node.h"

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
        
        // Session Index
        static int s_index = 0;

        // Our incoming data object
        int dataType = 0;

        // Frame index in RenderBuffers
        int f_index = 0;
        
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
            }
            catch( ... )
            {
                break;
            }
            
            // Handle the data we received
            switch (dataType)
            {
                case 0: // Open a new image
                {
                    DataHeader dh = node->m_server.listenHeader();
                    
                    // Copy data from d
                    const int& _index = dh.index();
                    const int& _xres = dh.xres();
                    const int& _yres = dh.yres();
                    const long long& _area = dh.rArea();
                    const int& _version = dh.version();
                    const double& _frame = static_cast<double>(dh.currentFrame());
                    const float& _fov = dh.camFov();
                    const Matrix4& _matrix = Matrix4(&dh.camMatrix()[0]);
                    const std::vector<int> _samples = dh.samples();
                    
                    // Get image area to calculate the progress
                    regionArea = _area;
                    
                    // Get delta time per IPR iteration
                    delta_time = _active_time;
                    
                    // Set current frame
                    node->m_current_frame = _frame;

                    // Adding new session
                    if (node->m_framebuffers.empty() || s_index != _index)
                    {
                        FrameBuffer fb;
                        WriteGuard lock(node->m_mutex);
                        node->m_framebuffers.push_back(fb);
                        node->m_output.push_back(node->getDateTime());
                        s_index = _index;
                    }
                    
                    FrameBuffer& fb = node->m_framebuffers.back();

                    // Create RenderBuffer
                    if (node->m_multiframes)
                    {
                        if (!fb.frame_exists(_frame))
                        {
                            WriteGuard lock(node->m_mutex);
                            fb.add(_frame, _xres, _yres);
                        }
                    }
                    else
                    {
                        if (!fb.empty())
                        {
                            WriteGuard lock(node->m_mutex);
                            fb.clear_all_apart(_frame);
                        }
                    }
                    
                    // Get current RenderBuffer
                    RenderBuffer& rb = fb.get_frame(_frame);
                    
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
                    
                    // Setting Camera
                    if (rb.isCameraChanged(_fov, _matrix))
                    {
                        WriteGuard lock(node->m_mutex);
                        rb.setCamera(_fov, _matrix);
                        node->setCameraKnobs(rb.getCameraFov(),
                                             rb.getCameraMatrix());
                    }

                    // Set Version
                    if (rb.getVersionInt() != _version)
                        rb.setVersion(_version);
                    
                    // Set Samples
                    if (rb.getSamplesInt() != _samples)
                        rb.setSamples(_samples);
                    
                    // Reset active AOVs
                    if(!active_aovs.empty())
                        active_aovs.clear();
                    break;
                }
                case 1: // Write image data
                {
                    DataPixels dp = node->m_server.listenPixels();

                    // Get Render Buffer
                    FrameBuffer& fb = node->m_framebuffers.back();
                    RenderBuffer& rb = fb.get_frame(node->m_current_frame);
                    
                    const char* _aov_name = dp.aovName();
                    const int& _xres = dp.xres();
                    const int& _yres = dp.yres();

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
                        // Get data from d
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
                        if(!rb.isBufferExist(_aov_name) && (node->m_enable_aovs || rb.empty()))
                            rb.addBuffer(_aov_name, _spp);
                        else
                            rb.ready(true);
                        
                        // Get buffer index
                        const int b = rb.getBufferIndex(_aov_name);
                    
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
                            node->setCurrentFrame(node->m_current_frame);
                            node->flagForUpdate(box);
                        }
                    }
                    dp.free();
                    break;
                }
                case 2: // Close image
                {
                    std::cout << "Close Image!" << std::endl;
                    break;
                }
                case 9: // This is sent when the parent process want to kill
                        // the listening thread
                {
                    std::cout << "Quit!" << std::endl;
                    killThread = true;
                    break;
                }
            }
        }
    }
}

#endif /* FBWriter_h */
