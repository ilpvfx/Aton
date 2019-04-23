/*
Copyright (c) 2019,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FBWriter_h
#define FBWriter_h

#define PRINT(var) std::cout << var << std::endl;

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
                    const int& _version = dh.version();
                    const float& _fov = dh.camera_fov();
                    const char* _name = dh.output_name();
                    const long long& _session = dh.session();
                    const std::vector<int> _samples = dh.samples();
                    const long long& _region_area = dh.region_area();
                    const double& _frame = static_cast<double>(dh.frame());
                    const Matrix4& _matrix = Matrix4(&dh.camera_matrix()[0]);

                    // Get FrameBuffer
                    std::vector<FrameBuffer>& fbs = node->m_framebuffers;
                    
                    WriteGuard lock(node->m_mutex);
                    fb = node->get_framebuffer(_session);
                    bool& multiframe = node->m_multiframes;
                    
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
                                fb->update_renderbuffer(&dh);
                        }
                    }
                    
                    if (fbs.empty())
                    {
                        fb = node->add_framebuffer();
                        rb = fb->add_renderbuffer(&dh);
                    }
                    
                    // Set FrameBuffer frame
                    node->set_current_frame(_frame);
                    if (fb->frame_changed(_frame))
                        fb->set_frame(_frame);
                    
                    // Get current RenderBuffer
                    if (rb == NULL)
                        rb = fb->get_renderbuffer(_frame);
                    
                    // Update Name
                    if (rb->name_changed(_name))
                        rb->set_name(_name);
                    
                    // Update Frame
                    if (rb->frame_changed(_frame))
                        rb->set_frame(_frame);
                    
                    // Update Camera
                    if (rb->camera_changed(_fov, _matrix))
                        rb->set_camera(_fov, _matrix);
                    
                    // Update Version
                    if (rb->get_version_int() != _version)
                        rb->set_version(_version);
                    
                    // Update Samples
                    if (rb->get_samples_int() != _samples)
                        rb->set_samples(_samples);
                    
                    // Update Region Area
                    rb->set_region_area(_region_area);
                    
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
                    break;
                }
                case 1: // Write image data
                {
                    // Get Data Pixels
                    DataPixels dp = node->m_server.listenPixels();
                    
                    const int& _xres = dp.xres();
                    const int& _yres = dp.yres();
                    const char* _aov_name = dp.aov_name();
                    const long long& _session = dp.session();

                    // Get Render Buffer
                    WriteGuard lock(node->m_mutex);
                    fb = node->get_framebuffer(_session);
                    
                    if (fb == NULL)
                        fb = &node->m_framebuffers.back();
                    
                    rb = fb->get_renderbuffer(fb->get_frame());

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
                        const int& _spp = dp.spp();
                        const int& _time = dp.time();
                        const int& _x = dp.bucket_xo();
                        const int& _y = dp.bucket_yo();
                        const long long& _ram = dp.ram();
                        const int& _width = dp.bucket_size_x();
                        const int& _height = dp.bucket_size_y();

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
                        if(rb->first_aov_name(_aov_name) && !node->m_capturing)
                        {
                            if (node->current_fb_index() == 0 ||
                                node->current_framebuffer() == fb ||
                                node->m_output_changed == Aton::item_added)
                            {
                                // Set status parameters
                                rb->set_time(_time);
                                rb->set_memory(_ram);
                                rb->set_progress(_width * _height);

                                // Update the image
                                const Box box = Box(_x, h - _y - _width, _x + _height, h - _y);
                                node->flag_update(box);
                            }
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
