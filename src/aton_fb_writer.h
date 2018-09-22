/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FBWriter_h
#define FBWriter_h

#include "aton_node.h"

// Our RenderBuffer writer thread
static void fb_writer(unsigned index, unsigned nthreads, void* data)
{
    bool killThread = false;
    Aton* node = reinterpret_cast<Aton*> (data);

    while (!killThread)
    {
        // Accept incoming connections!
        node->m_server.accept();

        // Data pointers
        FrameBuffer* fb = NULL;
        RenderBuffer* rb = NULL;
        
        // Our incoming data object
        int data_type = 0;
        
        // Session Index
        long long session = 0;
        
        // Time to reset per every IPR iteration
        int active_time = 0, delta_time = 0;
        
        // For progress percentage
        long long progress = 0, region_area = 0, rendered_area = 0;
        
        // Active Aovs names holder
        std::vector<std::string> active_aovs;
        
        // Loop over incoming data
        while (data_type != 2 || data_type != 9)
        {
            // Listen for some data
            try
            {
                data_type = node->m_server.listen_type();
                WriteGuard lock(node->m_mutex);
                node->m_running = true;
            }
            catch( ... )
            {
                WriteGuard lock(node->m_mutex);
                node->m_running = false;
                node->flag_update();
                break;
            }
            
            // Handle the data we received
            switch (data_type)
            {
                case 0: // Open a new image
                {
                    // Get Data Header
                    DataHeader dh = node->m_server.listenHeader();

                    // Get Current Session Index
                    session = dh.session();
                    
                    // Get image area to calculate the progress
                    region_area = dh.region_area();
                    rendered_area = dh.region_area();
                    
                    // Set Frame on Timeline
                    const double& _frame = static_cast<double>(dh.frame());
                    node->set_current_frame(_frame);

                    bool& multiframe = node->m_multiframes;
                    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
                
                    // Get FrameBuffer
                    WriteGuard lock(node->m_mutex);
                    fb = node->get_framebuffer(session);
                    
                    if (multiframe)
                    {
                        if (!fbs.empty())
                        {
                            if (fb == NULL)
                                fb = &fbs.back();
                            
                            if (!fb->renderbuffer_exists(_frame))
                            {
                                rb = fb->add_renderbuffer(&dh);
                                node->m_output_changed = Aton::item_added;
                            }
                            else
                            {
                                fb->update_renderbuffer(&dh);
                                node->m_output_changed = Aton::item_added;
                            }
                        }
                    }
                    else
                    {
                        if (!fbs.empty())
                        {
                            if (fb == NULL)
                            {
                                fb = node->add_framebuffer();
                                rb = fb->add_renderbuffer(&dh);
                            }
                            else
                            {
                                fb->update_renderbuffer(&dh);
                                node->m_output_changed = Aton::item_added;
                            }
                        }
                    }
                    
                    if (fbs.empty())
                    {
                        fb = node->add_framebuffer();
                        rb = fb->add_renderbuffer(&dh);
                    }
                    
                    // Get current RenderBuffer
                    if (rb == NULL)
                        rb = fb->get_renderbuffer(_frame);
                    
                    // Update Frame
                    if (rb->frame_changed(_frame))
                        rb->set_frame(_frame);
                    
                    // Update Camera
                    const float& _fov = dh.camera_fov();
                    const Matrix4& _matrix = Matrix4(&dh.camera_matrix()[0]);
                    if (rb->camera_changed(_fov, _matrix))
                        rb->set_camera(_fov, _matrix);
                    
                    // Update Version
                    const int& _version = dh.version();
                    if (rb->get_version_int() != _version)
                        rb->set_version(_version);
                    
                    // Update Samples
                    const std::vector<int> _samples = dh.samples();
                    if (rb->get_samples_int() != _samples)
                        rb->set_samples(_samples);
                    
                    // Update AOVs
                    if (!active_aovs.empty())
                    {
                        if(rb->aovs_changed(active_aovs))
                        {
                            rb->resize(1);
                            rb->set_ready(false);
                            node->reset_channels(node->m_channels);
                        }
                        active_aovs.clear();
                    }
                    
                    // Get delta time per IPR iteration
                    delta_time = active_time;

                    break;
                }
                case 1: // Write image data
                {
                    // Get Data Pixels
                    DataPixels dp = node->m_server.listenPixels();
                    
                    const int& _xres = dp.xres();
                    const int& _yres = dp.yres();
                    const char* _aov_name = dp.aov_name();

                    // Get Render Buffer
                    WriteGuard lock(node->m_mutex);
                    if(rb->resolution_changed(_xres, _yres))
                        rb->set_resolution(_xres, _yres);

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
                        active_time = _time;

                        // Adding buffer
                        if(!rb->aov_exists(_aov_name) && (node->m_enable_aovs || rb->empty()))
                            rb->add_aov(_aov_name, _spp);
                        else
                            rb->set_ready(true);

                        // Get RenderBuffer height
                        const int& h = rb->get_height();

                        // Get buffer index
                        const int b = rb->get_aov_index(_aov_name);

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
                                    rb->set_aov_pix(b, xpos, ypos, _spp, c, _pix);
                                }
                            }
                        }

                        // Update only on first aov
                        if(!node->m_capturing && rb->first_aov_name(_aov_name))
                        {
                            // Calculate the progress percentage
                            rendered_area -= _width * _height;
                            progress = 100 - (rendered_area * 100) / region_area;
                            
                            // Set status parameters
                            rb->set_progress(progress);
                            rb->set_memory(_ram);
                            rb->set_time(_time, delta_time);

                            // Update the image
                            const Box box = Box(_x, h - _y - _width, _x + _height, h - _y);
                            node->flag_update(box);
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
