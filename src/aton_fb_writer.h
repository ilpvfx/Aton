/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
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

        // Our incoming data object
        int dataType = 0;
        
        // Session Index
        static long long session_idx = 0;
        
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
                break;
            }
            
            // Handle the data we received
            switch (dataType)
            {
                case 0: // Open a new image
                {
                    DataHeader dh = node->m_server.listenHeader();

                    // Copy data from d
                    const long long& _index = dh.index();
                    const int& _xres = dh.xres();
                    const int& _yres = dh.yres();
                    const long long& _area = dh.rArea();
                    const int& _version = dh.version();
                    const double& _frame = static_cast<double>(dh.currentFrame());
                    const float& _fov = dh.camFov();
                    const Matrix4& _matrix = Matrix4(&dh.camMatrix()[0]);
                    const std::vector<int> _samples = dh.samples();
                    const char* _output_name = dh.outputName();

                    // Get image area to calculate the progress
                    regionArea = _area;

                    // Get delta time per IPR iteration
                    delta_time = _active_time;

                    std::vector<std::string>& output = node->m_output;
                    std::vector<long long>& sessions = node->m_sessions;
                    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
                    
                    // Session Handling
                    long fb_index = 0;
                    bool new_session = true;
                    if (!sessions.empty())
                    {
                        fb_index = std::find(sessions.begin(),
                                             sessions.end(), _index) - sessions.begin();
                    
                        if (fb_index != sessions.size())
                            new_session = false;
                    }
                    
                    // Adding new session
                    if (fbs.empty() || (!node->m_multiframes && new_session))
                    {
                        FrameBuffer fb;
                        WriteGuard lock(node->m_mutex);
                        
                        fbs.push_back(fb);
                        node->m_sessions.push_back(_index);
                        output.push_back(_output_name + std::string("_") + node->getDateTime());
                        
                        session_idx = _index;
                        node->m_outputKnobChanged = Aton::item_added;
                    }

                    FrameBuffer& fb = fbs[fb_index];

                    // Create RenderBuffer
                    if (node->m_multiframes)
                    {
                        if (!fb.frameExists(_frame))
                        {
                            WriteGuard lock(node->m_mutex);
                            fb.addFrame(_frame, _xres, _yres);
                        }
                    }
                    else
                    {
                        if (fb.empty())
                        {
                            WriteGuard lock(node->m_mutex);
                            fb.addFrame(_frame, _xres, _yres);
                        }
                        else if (fb.size() > 1)
                        {
                            WriteGuard lock(node->m_mutex);
                            fb.clearAllExcept(_frame);
                        }
                    }
                    
                    // Set Current Frame
                    node->setCurrentFrame(_frame);

                    // Get current RenderBuffer
                    RenderBuffer& rb = fb.getFrame(_frame);

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
                    std::vector<long long>& sessions = node->m_sessions;
                    long fb_index = std::find(sessions.begin(),
                                             sessions.end(), session_idx) - sessions.begin();
                    
                    FrameBuffer& fb = node->m_framebuffers[fb_index];
                    RenderBuffer& rb = fb.getFrame(fb.currentFrame());

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
                            if (progress >= 100)
                                node->m_running = false;
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
                    WriteGuard lock(node->m_mutex);
                    node->m_running = false;
                    node->flagForUpdate();
                    break;
                }
                case 9: // This is sent when the parent process want to kill
                        // the listening thread
                {
                    killThread = true;
                    break;
                }
            }
        }
    }
}

#endif /* FBWriter_h */
