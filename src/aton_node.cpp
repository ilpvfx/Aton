/*
Copyright (c) 2019,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include "aton_node.h"
#include "aton_fb_writer.h"
#include "aton_fb_updater.h"

#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

void Aton::attach()
{
    m_legit = true;

    // Default status bar
    set_status();
    
    // Default Render Region
    reset_region_cmd();
    
    // We don't need to see these knobs
    knob("formats_knob")->hide();
    knob("capturing_knob")->hide();
    knob("cam_fov_knob")->hide();
    
    // Reset Snapshots
    m_node->m_outputKnob->tableKnob()->deleteAllItems();
    m_node->m_outputKnob->tableKnob()->reset();

    for (int i=0; i<16; ++i)
    {
        std::string knob_name = (boost::format("cM%s")%i).str();
        knob(knob_name.c_str())->hide();
    }

    // Construct full path for capturing
    using namespace boost::filesystem;
    m_node_name = node_name();
    path dir = get_path();
    path file = m_node_name + ".exr";
    std::string str_path = (dir / file).string();
    boost::replace_all(str_path, "\\", "/");
    knob("path_knob")->set_text(str_path.c_str());

    // Check if the format already exist
    for (int i = 0; i < Format::size(); ++i)
    {
        const char* f_name = Format::index(i)->name();
        if (f_name != NULL && m_node_name == f_name)
            m_format_exists = true;
    }

    if (!m_format_exists)
        m_fmt.add(m_node_name.c_str());
}

void Aton::detach()
{
    // Even though a node still exists once removed from a scene (in the
    // undo stack) we should close the port and reopen if attach() gets called.
    m_legit = false;
    disconnect();
    WriteGuard lock(m_node->m_mutex);
    m_node->m_framebuffers = std::vector<FrameBuffer>();
}

void Aton::append(Hash& hash)
{
    hash.append(m_node->m_hash_count);
    hash.append(uiContext().frame());
    hash.append(outputContext().frame());
}

void Aton::_validate(bool for_real)
{
    if (!m_node->m_server.connected() && !m_inError && m_legit)
        change_port(m_port);
    
    // Handle any connection error
    if (m_inError)
        error(m_connection_error.c_str());
   
    // Update Outputs
    set_outputs();
    
    ReadGuard lock(m_node->m_mutex);
    RenderBuffer* rb = current_renderbuffer();

    if (rb != NULL && !rb->empty() && rb->ready())
    {
        // Update Format
        set_format(rb->get_width(),
                   rb->get_height(),
                   rb->get_pixel_aspect());
        
        // Update Channels
        set_channels(rb->get_aovs(),
                     rb->ready());
        
        // Udpate Status Bar
        set_status(rb->get_progress(),
                   rb->get_memory(),
                   rb->get_peak_memory(),
                   rb->get_frame(),
                   rb->get_time_str(),
                   rb->get_name(),
                   rb->get_version_str(),
                   rb->get_samples());
        
        // Update Camera
        set_camera(rb->get_camera_fov(),
                   rb->get_camera_matrix());

        FrameBuffer* fb = current_framebuffer();
        info_.setFirstFrame(fb->get_first_frame());
        info_.setLastFrame(fb->get_last_frame());
    }

    // Setup format etc
    info_.format(*m_node->m_fmtp.format());
    info_.full_size_format(*m_node->m_fmtp.fullSizeFormat());
    info_.channels(m_node->m_channels);
    info_.set(m_node->info().format());
}

const MetaData::Bundle& Aton::_fetchMetaData(const char* keyname)
{
    m_node->m_metadata = Iop::_fetchMetaData(keyname);
    
    RenderBuffer* rb = current_renderbuffer();
   
    if (rb != NULL)
    {
        m_node->m_metadata.setData(std::string("exr/aton/name"), rb->get_name());
        m_node->m_metadata.setData(std::string("exr/aton/frame"), rb->get_frame());
        m_node->m_metadata.setData(std::string("exr/aton/time"), rb->get_time_str());
        m_node->m_metadata.setData(std::string("exr/aton/memory"), int(rb->get_peak_memory()));
        m_node->m_metadata.setData(std::string("exr/aton/sampling"), rb->get_samples());
        m_node->m_metadata.setData(std::string("exr/aton/version"), rb->get_version_str());
    }
    
    return m_node->m_metadata;
}

void Aton::engine(int y, int x, int r, ChannelMask channels, Row& out)
{
    ReadGuard lock(m_node->m_mutex);
    RenderBuffer* rb = current_renderbuffer();
    
    int w = 0, h = 0;
    if (rb != NULL)
    {
        w = rb->get_width();
        h = rb->get_height();
    }
    
    foreach(z, channels)
    {
        int b = 0;
        int xx = x;
        const int c = colourIndex(z);
        float* cOut = out.writable(z) + x;
        const float* END = cOut + (r - x);
        
        if (m_enable_aovs && rb != NULL && rb->ready())
            b = rb->get_aov_index(z);
        
        while (cOut < END)
        {
            if (rb == NULL || !rb->ready() || x >= w || y >= h || r > w)
                *cOut = 0.0f;
            else
                *cOut = rb->get_aov_pix(b, xx, y, c);
            ++cOut;
            ++xx;
        }
    }
}

void Aton::knobs(Knob_Callback f)
{
    // Listen knobs
    Divider(f, "Listen");
    Int_knob(f, &m_port, "port_knob", "Port");
    Knob* reset_knob = Button(f, "reset_port_knob", "Reset");
    
    // Camera knobs
    Divider(f, "Camera");
    Knob* live_cam_knob = Bool_knob(f, &m_live_camera, "live_camera_knob", "Create Live Camera");
    
    // Sanpshots
    Divider(f, "Snapshots");
    Bool_knob(f, &m_enable_aovs, "enable_aovs_knob", "Enable AOVs");
    Bool_knob(f, &m_multiframes, "multi_frame_knob", "Multiple Frames Mode");
    m_outputKnob = Table_knob(f, "output_knob", "Output");
    if (f.makeKnobs())
    {
        Table_KnobI* outputKnob = m_outputKnob->tableKnob();
        outputKnob->addStringColumn("snapshots", "", true, 512);
    }
    Newline(f);
    Knob* snapshot = Button(f, "take_snapshot_knob", "<img src=\":qrc/images/ScriptEditor/load.png\">");
    Knob* move_up = Button(f, "move_up_knob", "<img src=\":qrc/images/ScriptEditor/inputOff.png\">");
    Knob* move_down = Button(f, "move_down_knob", "<img src=\":qrc/images/ScriptEditor/outputOff.png\">");
    Knob* remove_selectd = Button(f, "remove_selected_knob", "<img src=\":qrc/images/ScriptEditor/clearOutput.png\">");
    
    // Render Region
    Divider(f, "Render Region");
    Knob* region_knob = BBox_knob(f, m_region, "region_knob", "Area");
    Newline(f);
    Button(f, "reset_region_knob", "Reset");
    Button(f, "fit_region_knob", "Fit");
    Button(f, "copy_region_knob", "Copy");
    
    // Write knobs
    Divider(f, "Write to Disk");
    Knob* write_multi_frame_knob = Bool_knob(f, &m_write_frames, "write_multi_frame_knob", "Write Multiple Frames");
    Knob* path_knob = File_knob(f, &m_path, "path_knob", "Path");
    Newline(f);
    Button(f, "render_knob", "Render");
    Button(f, "import_latest_knob", "Read Latest");
    Button(f, "import_all_knob", "Read All");
    
    // Status Bar
    BeginToolbar(f, "status_bar");
    Knob* statusKnob = String_knob(f, &m_status, "status_knob", "");
    EndToolbar(f);
    
    // Hidden knobs
    Format_knob(f, &m_fmtp, "formats_knob", "format");
    Bool_knob(f, &m_capturing, "capturing_knob");
    Float_knob(f, &m_cam_fov, "cam_fov_knob", " cFov");
    
    for (int i=0; i<16; ++i)
    {
        std::string knob_name = (boost::format("cM%s")%i).str();
        Float_knob(f, &m_cam_matrix, knob_name.c_str(), knob_name.c_str());
    }
    
    // Setting Flags
    reset_knob->set_flag(Knob::NO_RERENDER, true);
    path_knob->set_flag(Knob::NO_RERENDER, true);
    live_cam_knob->set_flag(Knob::NO_RERENDER, true);
    move_up->set_flag(Knob::NO_RERENDER, true);
    snapshot->set_flag(Knob::NO_RERENDER, true);
    move_down->set_flag(Knob::NO_RERENDER, true);
    remove_selectd->set_flag(Knob::NO_RERENDER, true);
    write_multi_frame_knob->set_flag(Knob::NO_RERENDER, true);
    region_knob->set_flag(Knob::NO_RERENDER, true);
    statusKnob->set_flag(Knob::NO_RERENDER, true);
    statusKnob->set_flag(Knob::DISABLED, true);
    statusKnob->set_flag(Knob::READ_ONLY, true);
    statusKnob->set_flag(Knob::OUTPUT_ONLY, true);
}

int Aton::knob_changed(Knob* _knob)
{
    if (_knob->is("reset_port_knob"))
    {
        if (m_port != 9201)
            change_port(9201);
        return 1;
    }
    if (_knob->is("port_knob"))
    {
        change_port(m_port);
        return 1;
    }
    if (_knob->is("output_knob"))
    {
        select_output_cmd();
        return 1;
    }
    if (_knob->is("move_up_knob"))
    {
        move_cmd(true);
        return 1;
    }
    if (_knob->is("move_down_knob"))
    {
        move_cmd(false);
        return 1;
    }
    if (_knob->is("remove_selected_knob"))
    {
        remove_selected_cmd();
        return 1;
    }
    if (_knob->is("take_snapshot_knob"))
    {
        snapshot_cmd();
        return 1;
    }
    if (_knob->is("multi_frame_knob"))
    {
        multiframe_cmd();
        return 1;
    }
    if (_knob->is("live_camera_knob"))
    {
        live_camera_toogle();
        return 1;
    }
    if (_knob->is("reset_region_knob"))
    {
        reset_region_cmd();
        return 1;
    }
    if (_knob->is("fit_region_knob"))
    {
        fit_region_cmd();
        return 1;
    }
    if (_knob->is("copy_region_knob"))
    {
        copy_region_cmd();
        return 1;
    }
    if (_knob->is("render_knob"))
    {
        capture_cmd();
        return 1;
    }
    if (_knob->is("import_latest_knob"))
    {
        import_cmd(false);
        return 1;
    }
    if (_knob->is("import_all_knob"))
    {
        import_cmd(true);
        return 1;
    }
    return 0;
}

// We can use this to change our tcp port
void Aton::change_port(int port)
{
    m_legit = false;
    m_inError = false;

    // Try to reconnect
    disconnect();

    try
    {
        m_server.connect(port, true);
        m_legit = true;
    }
    catch ( ... )
    {
        std::stringstream stream;
        stream << "Could not connect to port: " << port;
        m_connection_error = stream.str();
        m_inError = true;
        print_name( std::cerr );
        std::cerr << ": " << stream.str() << std::endl;
        return;
    }

    // Success
    if (m_server.connected())
    {
        Thread::spawn(::fb_writer, 1, m_node);

        // Update port in the UI
        if (m_port != m_server.get_port())
        {
            std::stringstream stream;
            stream << (m_server.get_port());
            std::string port = stream.str();
            knob("port_knob")->set_text(port.c_str());
        }
    }
}

int Aton::get_port()
{
    const char* def_port = getenv("ATON_PORT");
    int aton_port;
    
    if (def_port == NULL)
        aton_port = 9201;
    else
        aton_port = atoi(def_port);
    
    return aton_port;
}

std::string Aton::get_path()
{
    char* aton_path = getenv("ATON_CAPTURE_PATH");
    
    // Get OS specific tmp directory path
    using namespace boost::filesystem;
    std::string def_path = temp_directory_path().string();
    
    if (aton_path != NULL)
        def_path = aton_path;
    
    boost::replace_all(def_path, "\\", "/");
    
    return def_path;
}

// Disconnect the server for it's port
void Aton::disconnect()
{
    if (m_server.connected())
    {
        m_server.quit();
        Thread::wait(m_node);
    }
}

void Aton::flag_update(const Box& box)
{
    if (m_node->m_hash_count == UINT_MAX)
        m_node->m_hash_count = 0;
    else
        m_node->m_hash_count++;

    // Update the image with current bucket if given
    asapUpdate(box);
}

FrameBuffer* Aton::get_framebuffer(const long long& session)
{
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    if (!fbs.empty())
    {
        std::vector<FrameBuffer>::iterator it;
        for(it = fbs.begin(); it != fbs.end(); ++it)
            if (it->get_session() == session)
                return &(*it);
    }
    return NULL;
}

FrameBuffer* Aton::add_framebuffer()
{
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    fbs.push_back(FrameBuffer());
    m_node->m_output_changed = Aton::item_added;
    return &fbs.back();
}

FrameBuffer* Aton::current_framebuffer()
{
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    
    if (!fbs.empty())
    {
        const int idx = m_node->current_fb_index(false);
        if (idx < 0)
            return &fbs[0];
        else
            return &fbs[idx];
    }
    return NULL;
}

RenderBuffer* Aton::current_renderbuffer()
{
    FrameBuffer* fb = current_framebuffer();
    
    if (fb != NULL)
    {
        double frame;
        if  (m_multiframes)
            frame = uiContext().frame();
        else
            frame = fb->get_frame();
        return fb->get_renderbuffer(frame);
    }
    else
        return NULL;
}

int Aton::current_fb_index(bool direction)
{
    Table_KnobI* outputKnob = m_node->m_outputKnob->tableKnob();

    const int size = outputKnob->getRowCount();
    const int idx = outputKnob->getSelectedRow();

    if (size > 0 && idx >= 0 && !direction)
        return size - idx - 1;
    
    return idx;
}

std::vector<int> Aton::selected_fb_indexes()
{
    Table_KnobI* outputKnob = m_node->m_outputKnob->tableKnob();
    
    std::vector<int> indexes;
    const int size = outputKnob->getRowCount();
    std::vector<int> rows = outputKnob->getSelectedRows();

    std::vector<int>::iterator it;
    for(it = rows.begin(); it != rows.end(); ++it)
    {
        if (size > 0 && *it >= 0)
            indexes.push_back(size - *it - 1);
        else
            indexes.push_back(0);
    }
    
    return indexes;
}

void Aton::set_outputs()
{
    // Setup dynamic knob
    WriteGuard lock(m_node->m_mutex);
    int& knob_changed = m_node->m_output_changed;
    
    if (knob_changed)
    {    
        std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
        Table_KnobI* outputKnob = m_node->m_outputKnob->tableKnob();

        int idx = current_fb_index();
        
        if (idx >= 0)
        {
            switch (knob_changed)
            {
                case Aton::item_added: idx = 0;
                    break;
                case Aton::item_moved_up: --idx;
                    break;
                case Aton::item_moved_down: ++idx;
                    break;
                case Aton::item_removed:
                {
                    if (idx >= fbs.size())
                        idx = static_cast<int>(fbs.size() - 1);
                    break;
                }
            }
        }
        else
            idx = 0;
        
        outputKnob->deleteAllItems();
        
        if (!fbs.empty())
        {
            std::vector<FrameBuffer>::reverse_iterator it;
            for(it = fbs.rbegin(); it != fbs.rend(); ++it)
            {
                const int row = outputKnob->addRow();
                outputKnob->setCellString(row, 0, it->get_output_name());
            }
        }
        
        outputKnob->reset();
        outputKnob->selectRow(idx);
        knob_changed = Aton::item_not_changed;
    }
}

void Aton::set_format(const int& width,
                      const int& height,
                      const float& pixel_aspect)
{
    // Set the format
    if (m_node->m_fmt.width() != width ||
        m_node->m_fmt.height() != height ||
        m_node->m_fmt.pixel_aspect() != pixel_aspect)
    {
        Format* m_fmt_ptr = &m_node->m_fmt;
        if (m_node->m_format_exists)
        {
            bool fmtFound = false;
            unsigned int i;
            for (i=0; i < Format::size(); ++i)
            {
                const char* f_name = Format::index(i)->name();
                if (f_name != NULL && m_node->m_node_name == f_name)
                {
                    m_fmt_ptr = Format::index(i);
                    fmtFound = true;
                }
            }
            if (!fmtFound)
                m_fmt_ptr->add(m_node->m_node_name.c_str());
        }
        
        m_fmt_ptr->set(0, 0, width, height);
        m_fmt_ptr->width(width);
        m_fmt_ptr->height(height);
        m_fmt_ptr->pixel_aspect(pixel_aspect);
        knob("formats_knob")->set_text(m_node->m_node_name.c_str());
    }
}

void Aton::set_channels(std::vector<std::string>& aovs,
                        const bool& ready)
{
    // Set the channels
    ChannelSet& channels = m_node->m_channels;
    
    if (m_enable_aovs && !aovs.empty() && ready)
    {
        if (channels.size() != aovs.size())
            channels.clear();
        
        std::vector<std::string>::iterator it;
        for(it = aovs.begin(); it != aovs.end(); ++it)
        {
            using namespace chStr;
            if (*it == RGBA && !channels.contains(Chan_Red))
            {
                channels.insert(Chan_Red);
                channels.insert(Chan_Green);
                channels.insert(Chan_Blue);
                channels.insert(Chan_Alpha);
                continue;
            }
            else if (*it == Z && !channels.contains(Chan_Z))
            {
                channels.insert(Chan_Z);
                continue;
            }
            else if (*it == N || *it == P)
            {
                if (!channels.contains(channel((*it + _X).c_str())))
                {
                    channels.insert(channel((*it + _X).c_str()));
                    channels.insert(channel((*it + _Y).c_str()));
                    channels.insert(channel((*it + _Z).c_str()));
                }
                continue;
            }
            else if (*it == ID)
            {
                if (!channels.contains(channel((*it + _red).c_str())))
                    channels.insert(channel((*it + _red).c_str()));
                continue;
            }
            else if (!channels.contains(channel((*it + _red).c_str())))
            {
                channels.insert(channel((*it + _red).c_str()));
                channels.insert(channel((*it + _green).c_str()));
                channels.insert(channel((*it + _blue).c_str()));
            }
        }
    }
    else
        reset_channels(channels);
}

void Aton::reset_channels(ChannelSet& channels)
{
    if (channels.size() > 4)
    {
        channels.clear();
        channels.insert(Chan_Red);
        channels.insert(Chan_Green);
        channels.insert(Chan_Blue);
        channels.insert(Chan_Alpha);
    }
}

void Aton::set_camera(const float& fov,
                      const Matrix4& matrix)
{
    const char* knob_name;
    for (int i=0, k=0; i<4; ++i)
    {
        for (int j=0; j<4; ++j)
        {
            knob_name = (boost::format("cM%s")%k++).str().c_str();
            knob(knob_name)->set_value(*(matrix[i]+j));
        }
    }
    knob("cam_fov_knob")->set_value(fov);
}

void Aton::set_current_frame(const double& frame)
{
    // Set Current Frame and update the UI
    OutputContext ctxt = outputContext();
    if (frame != ctxt.frame())
    {
        ctxt.setFrame(frame);
        gotoContext(ctxt, true);
    }
}

void Aton::set_status(const long long& progress,
                      const long long& ram,
                      const long long& p_ram,
                      const double& frame,
                      const char* time,
                      const char* name,
                      const char* version,
                      const char* samples)
{
    FrameBuffer* fb = current_framebuffer();
    
    size_t fb_size = fb == NULL ? 0 : fb->size();
    std::string status_str = (boost::format("Arnold %s | "
                                            "Memory: %sMB / %sMB | "
                                            "Time: %s | "
                                            "Name: %s | "
                                            "Frame: %s(%s) | "
                                            "Sampling: %s | "
                                            "Progress: %s%%")%version%ram%p_ram%time%name
                                                             %frame%fb_size%samples%progress).str();
    
    Knob* statusKnob = m_node->knob("status_knob");
    statusKnob->set_flag(Knob::DISABLED, (progress == 100) || !m_node->m_running);
    statusKnob->set_text(status_str.c_str());
}

void Aton::multiframe_cmd()
{
    WriteGuard lock(m_node->m_mutex);
    FrameBuffer* fb = current_framebuffer();
   
    if (fb != NULL)
        fb->set_frame(uiContext().frame());

    if (m_node->m_multiframes)
        Thread::spawn(::fb_updater, 1, m_node);
}

void Aton::select_output_cmd()
{
    m_node->m_mutex.writeLock();
    
    FrameBuffer* fb = current_framebuffer();
    Table_KnobI* outputKnob = m_outputKnob->tableKnob();
    
    const int idx = outputKnob->getSelectedRow();
    std::vector<int> sel_rows = outputKnob->getSelectedRows();
    
    if (fb != NULL && sel_rows.size() == 1 && idx >= 0)
    {
        // Check if item has renamed from UI
        if (m_node->m_output_changed == Aton::item_not_changed)
        {
            std::string row_name = outputKnob->getCellString(idx, 0);
            if (row_name != fb->get_output_name())
                fb->set_output_name(row_name);
        }
        
        const double& frame = fb->get_frame();
        m_node->m_mutex.unlock();
        
        // Update UI Frame
        set_current_frame(frame);
        flag_update();
    }
    else
        m_node->m_mutex.unlock();
}

void Aton::snapshot_cmd()
{
    WriteGuard lock(m_node->m_mutex);
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    
    if (!fbs.empty())
    {   
        FrameBuffer* fb = current_framebuffer();
        int idx = current_fb_index(false);
        
        if (fb != NULL && idx >= 0)
        {
            idx = idx > 0 ? idx-- : 0;
            fbs.insert(fbs.begin() + idx, *fb);
            fbs[idx].set_session(0);
            m_node->m_output_changed = Aton::item_copied;
            flag_update();
        }
    }
}

void Aton::move_cmd(bool direction)
{
    WriteGuard lock(m_node->m_mutex);
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    if (!fbs.empty())
    {
        const int idx = m_node->current_fb_index(false);
        if (idx >= 0)
        {
            if (direction && idx < (fbs.size()-1))
            {
                std::swap(fbs[idx], fbs[idx+1]);
                m_node->m_output_changed = Aton::item_moved_up;;
            }
            else if (!direction && idx != 0)
            {
                std::swap(fbs[idx], fbs[idx-1]);
                m_node->m_output_changed = Aton::item_moved_down;
            }
            flag_update();
       }
    }
}

void Aton::remove_selected_cmd()
{
    WriteGuard lock(m_node->m_mutex);
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    
    if (!fbs.empty() && !m_node->m_running)
    {
        std::vector<int> indexes = selected_fb_indexes();
        
        std::vector<int>::iterator it;
        for(it = indexes.begin(); it != indexes.end(); ++it)
            if (*it >= 0)
                fbs.erase(fbs.begin() + *it);

        m_node->m_output_changed = Aton::item_removed;

        if (fbs.empty())
            set_status();

        flag_update();
    }
}

void Aton::reset_region_cmd()
{
    // Reset Region
    knob("region_knob")->set_value(0);
}

void Aton::fit_region_cmd()
{
    const double w = double(m_node->m_fmtp.format()->width());
    const double h = double(m_node->m_fmtp.format()->height());
    
    if (w > 0 && h > 0)
    {
        double fit_region[4] = {0, 0, w, h};
        knob("region_knob")->set_values(fit_region, 4);
    }
}
        
void Aton::copy_region_cmd()
{
    const int& w = m_node->m_fmtp.format()->width();
    const int& h = m_node->m_fmtp.format()->height();

    if (w > 0 && h > 0)
    {
        const float ux = m_region[0] / float(w);
        const float uy = m_region[1] / float(h);
        const float ur = m_region[2] / float(w);
        const float ut = m_region[3] / float(h);

        std::string cmd; // Our python command buffer
        cmd = (boost::format("exec('''try:\n\t"
                                         "from PySide import QtGui as QtWidgets\n"
                                     "except ImportError:\n\t"
                                         "from PySide2 import QtWidgets\n"
                                     "clipboard = QtWidgets.QApplication.clipboard()\n"
                                     "clipboard.setText('%s,%s,%s,%s')''')" )%ux%uy%ur%ut).str();
        script_command(cmd.c_str(), true, false);
        script_unlock();
    }
}

bool Aton::path_valid(std::string f_path)
{
    using namespace boost::filesystem;
    return exists(path(f_path).parent_path());
}

std::vector<std::string> Aton::get_captures()
{
    // Our captured filenames list
    using namespace boost::filesystem;
    std::vector<std::string> results;

    // If the directory exist
    if (path_valid(m_path))
    {    
        path filepath(m_path);
        // Regex expression to find captured files
        std::string exp = (boost::format("%s.+.%s")%filepath.stem().string()
                           %filepath.extension().string() ).str();
        const boost::regex filter(exp);
        
        // Iterating through directory to find matching files
        directory_iterator it(filepath.parent_path());
        BOOST_FOREACH(path const& p, std::make_pair(it, directory_iterator()))
        {
            if(is_regular_file(p))
            {
                boost::match_results<std::string::const_iterator> what;
                if (boost::regex_search(it->path().filename().string(),
                                        what, filter, boost::match_default))
                {
                    std::string res = p.filename().string();
                    results.push_back(res);
                }
            }
        }
    }
    return results;
}

void Aton::capture_cmd()
{
    ReadGuard lock(m_node->m_mutex);
    FrameBuffer* fb = current_framebuffer();

    if (fb != NULL && path_valid(m_path))
    {
        // Add date or frame suffix to the path
        std::string key (".");
        std::string timeFrameSuffix;
        std::string frames;
        double startFrame;
        double endFrame;
        
        std::vector<double> sortedFrames = fb->frames();
        std::stable_sort(sortedFrames.begin(), sortedFrames.end());

        if (m_multiframes && m_write_frames)
        {
            timeFrameSuffix = "_" + std::string("####");
            startFrame = sortedFrames.front();
            endFrame = sortedFrames.back();

            std::vector<double>::iterator it;
            for(it = sortedFrames.begin(); it != sortedFrames.end(); ++it)
                frames += (boost::format("%s,")%*it).str();

            frames.resize(frames.size() - 1);
        }
        else
        {
            timeFrameSuffix = "_" + fb->get_output_name();
            startFrame = endFrame = uiContext().frame();
            frames = (boost::format("%s")%uiContext().frame()).str();
        }

        timeFrameSuffix += ".";
        std::string path = std::string(m_path);
        std::size_t found = path.rfind(key);
        if (found != std::string::npos)
            path.replace(found, key.length(), timeFrameSuffix);

        std::string cmd; // Our python command buffer
        // Create a Write node and return it's name
        cmd = (boost::format("nuke.nodes.Write(file='%s').name()")%path.c_str()).str();
        script_command(cmd.c_str());
        std::string writeNodeName = script_result();
        script_unlock();

        // Connect to Write node
        cmd = (boost::format("nuke.toNode('%s').setInput(0, nuke.toNode('%s'));"
                             "nuke.toNode('%s')['channels'].setValue('all');"
                             "nuke.toNode('%s')['afterRender']."
                             "setValue('''nuke.nodes.Read(file='%s', first=%s, last=%s, on_error=3)''')")%writeNodeName
                                                                                                         %m_node->m_node_name
                                                                                                         %writeNodeName
                                                                                                         %writeNodeName
                                                                                                         %path.c_str()
                                                                                                         %startFrame
                                                                                                         %endFrame).str();
        script_command(cmd.c_str(), true, false);
        script_unlock();

        // Execute the Write node
        cmd = (boost::format("exec('''import thread\n"
                                     "def writer():\n\t"
                                         "def status(cap):\n\t\t"
                                             "nuke.toNode('%s')['capturing_knob'].setValue(cap)\n\t\t"
                                             "if not cap:\n\t\t\t"
                                                 "nuke.delete(nuke.toNode('%s'))\n\t"
                                         "nuke.executeInMainThread(status, args=True)\n\t"
                                         "nuke.executeInMainThread(nuke.execute, args=('%s', nuke.FrameRanges([%s])))\n\t"
                                         "nuke.executeInMainThread(status, args=False)\n"
                                     "thread.start_new_thread(writer,())''')")%m_node->m_node_name
                                                                              %writeNodeName
                                                                              %writeNodeName
                                                                              %frames).str();
        script_command(cmd.c_str(), true, false);
        script_unlock();
    }
}

void Aton::import_cmd(bool all)
{
    std::vector<std::string> captures = get_captures();
    if (!captures.empty())
    {
        using namespace boost::filesystem;
        path dir = path(m_path).parent_path();

        // Reverse iterating through vector
        std::vector<std::string>::reverse_iterator it;
        for(it = captures.rbegin(); it != captures.rend(); ++it)
        {
            if (all == false && it != captures.rbegin())
                continue;

            std::string str_path = (dir / *it).string();
            boost::replace_all(str_path, "\\", "/");

            std::string cmd; // Our python command buffer
            cmd = (boost::format("exec('''readNodes = nuke.allNodes('Read')\n"
                                          "exist = False\n"
                                          "if len(readNodes)>0:\n\t"
                                              "for i in readNodes:\n\t\t"
                                                  "if '%s' == i['file'].value():\n\t\t\t"
                                                      "exist = True\n"
                                           "if exist != True:\n\t"
                                              "nuke.nodes.Read(file='%s')''')")%str_path
                                                                               %str_path ).str();
            script_command(cmd.c_str(), true, false);
            script_unlock();
        }
    }
}

void Aton::live_camera_toogle()
{
    // Our python command buffer
    std::string cmd, focalExpr;

    if (m_live_camera)
    {
        // Set Focal Length
        focalExpr = (boost::format("%s.cam_fov_knob!=0?(haperture/(2*tan(pi*%s.cam_fov_knob/360))):this")%m_node->m_node_name
                                                                                                         %m_node->m_node_name).str();
        // Set Matrix
        cmd = (boost::format("exec('''cam = nuke.nodes.Camera(name='%s_Camera')\n"
                             "cam['haperture'].setValue(36)\n"
                             "cam['vaperture'].setValue(24)\n"
                             "cam['focal'].setExpression('%s')\n"
                             "cam['useMatrix'].setValue(True)\n"
                             "for i in range(0, 16):\n\t"
                                 "cam['matrix'].setExpression('%s.cM'+str(i), i)''')")%m_node->m_node_name
                                                                                      %focalExpr
                                                                                      %m_node->m_node_name).str();
    }
    else
        cmd = (boost::format("nuke.delete(nuke.toNode('%s_Camera'))")%m_node->m_node_name).str();

    script_command(cmd.c_str(), true, false);
    script_unlock();
}

// Nuke node builder
static Iop* constructor(Node* node){ return new Aton(node); }
const Iop::Description Aton::desc(CLASS, 0, constructor);
