/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "aton_node.h"
#include "aton_fb_writer.h"

#include "boost/format.hpp"
#include "boost/foreach.hpp"
#include "boost/regex.hpp"
#include "boost/filesystem.hpp"
#include "boost/algorithm/string.hpp"

void Aton::attach()
{
    m_legit = true;

    // Default status bar
    setStatus();

    // We don't need to see these knobs
    knob("formats_knob")->hide();
    knob("capturing_knob")->hide();
    knob("cam_fov_knob")->hide();

    for (int i=0; i<16; i++)
    {
        std::string knob_name = (boost::format("cM%s")%i).str();
        knob(knob_name.c_str())->hide();
    }
    
    // Construct full path for capturing
    m_node_name = node_name();
    using namespace boost::filesystem;
    path dir = getPath();
    path file = m_node_name + std::string(".exr");
    path fullPath = dir / file;
    std::string str_path = fullPath.string();
    boost::replace_all(str_path, "\\", "/");
    knob("path_knob")->set_text(str_path.c_str());
    
    // Check if the format is already exist
    unsigned int i;
    for (i = 0; i < Format::size(); ++i)
    {
        const char* f_name = Format::index(i)->name();
        if (f_name != NULL && m_node_name == f_name)
            m_formatExists = true;
    }
    
    if (!m_formatExists)
        m_fmt.add(m_node_name.c_str());
}

void Aton::detach()
{
    // Even though a node still exists once removed from a scene (in the
    // undo stack) we should close the port and reopen if attach() gets called.
    m_legit = false;
    disconnect();
    m_node->m_frames = std::vector<double>();
    m_node->m_framebuffers = std::vector<FrameBuffer>();
}

// We can use this to change our tcp port
void Aton::changePort(int port)
{
    m_inError = false;
    m_legit = false;
    m_connectionError = "";
    
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
        m_connectionError = stream.str();
        m_inError = true;
        print_name( std::cerr );
        std::cerr << ": " << stream.str() << std::endl;
        return;
    }

    // Success
    if (m_server.isConnected())
    {
        Thread::spawn(::FBWriter, 1, m_node);
        
        // Update port in the UI
        if (m_port != m_server.getPort())
        {
            std::stringstream stream;
            stream << (m_server.getPort());
            std::string port = stream.str();
            knob("port_number")->set_text(port.c_str());
        }
    }
}

// Disconnect the server for it's port
void Aton::disconnect()
{
    if (m_server.isConnected())
    {
        m_server.quit();
        Thread::wait(m_node);
    }
}

void Aton::flagForUpdate(const Box& box)
{
    if (m_node->m_hash_count == UINT_MAX)
        m_node->m_hash_count = 0;
    else
        m_node->m_hash_count++;
    
    // Update the image with current bucket if given
    asapUpdate(box);
}

void Aton::append(Hash& hash)
{
    hash.append(m_node->m_hash_count);
    hash.append(uiContext().frame());
    hash.append(outputContext().frame());
}

FrameBuffer& Aton::current_framebuffer()
{
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    SceneView_KnobI* outputKnob = m_node->m_outputKnob->sceneViewKnob();

    std::vector<unsigned int> itemList;
    outputKnob->getHighlightedItems(itemList);
    
    if (itemList.empty())
        return fbs[0];
    else
    {
        size_t idx = itemList[0];
        idx = m_node->m_output.size() - idx - 1;
        return fbs[idx];
    }
        
}

RenderBuffer& Aton::current_renderbuffer()
{
    FrameBuffer& fb = current_framebuffer();
    
    double frame;
    if  (m_multiframes)
        frame = outputContext().frame();
    else
        frame = fb.current_frame();
    return fb.get_frame(frame);
}

void Aton::_validate(bool for_real)
{
    if (!m_node->m_server.isConnected() && !m_inError && m_legit)
        changePort(m_port);

    // Handle any connection error
    if (m_inError)
        error(m_connectionError.c_str());
    
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    if (!fbs.empty())
    {
        // Setup dynamic knob
        std::vector<std::string> menu = m_node->m_output;
        std::reverse(menu.begin(), menu.end());
        SceneView_KnobI* outputKnob = m_node->m_outputKnob->sceneViewKnob();
        
        if (menu != outputKnob->getItemNames())
        {
            outputKnob->menu(menu);
            outputKnob->removeItems(menu);
            outputKnob->addItems(menu);
            
            // Selecting the top item
            std::vector<unsigned int> item = { 0 };
            outputKnob->setSelectedItems(item);
        }
        
        RenderBuffer& rb = current_renderbuffer();
        if (!rb.empty())
        {
            // Set the progress
            setStatus(rb.getProgress(),
                      rb.getRAM(),
                      rb.getPRAM(),
                      rb.getTime(),
                      rb.getFrame(),
                      rb.getVersion(),
                      rb.getSamples());
            
            // Set the format
            const int width = rb.getWidth();
            const int height = rb.getHeight();
            
            if (m_node->m_fmt.width() != width ||
                m_node->m_fmt.height() != height)
            {
                Format* m_fmt_ptr = &m_node->m_fmt;
                if (m_node->m_formatExists)
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
                knob("formats_knob")->set_text(m_node->m_node_name.c_str());
            }
            
            // Update Camera knobs
            if (m_node->m_live_camera)
            {
                RenderBuffer& rb = current_renderbuffer();
                m_node->setCameraKnobs(rb.getCameraFov(),
                                       rb.getCameraMatrix());
            }
            
            // Set the channels
            ChannelSet& channels = m_node->m_channels;
            
            if (m_enable_aovs && rb.isReady())
            {
                const int fb_size = static_cast<int>(rb.size());
                
                if (channels.size() != fb_size)
                    channels.clear();

                for(int i = 0; i < fb_size; ++i)
                {
                    std::string bfName = rb.getBufferName(i);
                    
                    using namespace chStr;
                    if (bfName == RGBA && !channels.contains(Chan_Red))
                    {
                        channels.insert(Chan_Red);
                        channels.insert(Chan_Green);
                        channels.insert(Chan_Blue);
                        channels.insert(Chan_Alpha);
                        continue;
                    }
                    else if (bfName == Z && !channels.contains(Chan_Z))
                    {
                        channels.insert(Chan_Z);
                        continue;
                    }
                    else if (bfName == N || bfName == P)
                    {
                        if (!channels.contains(channel((bfName + _X).c_str())))
                        {
                            channels.insert(channel((bfName + _X).c_str()));
                            channels.insert(channel((bfName + _Y).c_str()));
                            channels.insert(channel((bfName + _Z).c_str()));
                        }
                        continue;
                    }
                    else if (bfName == ID)
                    {
                        if (!channels.contains(channel((bfName + _red).c_str())))
                            channels.insert(channel((bfName + _red).c_str()));
                        continue;
                    }
                    else if (!channels.contains(channel((bfName + _red).c_str())))
                    {
                        channels.insert(channel((bfName + _red).c_str()));
                        channels.insert(channel((bfName + _green).c_str()));
                        channels.insert(channel((bfName + _blue).c_str()));
                    }
                }
            }
            else
                resetChannels(channels);
        }
    }
    
    // Setup format etc
    info_.format(*m_node->m_fmtp.format());
    info_.full_size_format(*m_node->m_fmtp.fullSizeFormat());
    info_.channels(m_node->m_channels);
    info_.set(m_node->info().format());
}

void Aton::engine(int y, int x, int r, ChannelMask channels, Row& out)
{
    FrameBuffer& fb = current_framebuffer();
    std::vector<RenderBuffer>& rbs = fb.get_buffers();
    std::vector<FrameBuffer>& fbs = m_node->m_framebuffers;
    
    int f = 0;
    if (!fbs.empty())
    {
        ReadGuard lock(m_mutex);
        if (!m_multiframes)
            f = fb.get_index(fb.current_frame());
        else
            f = fb.get_index(outputContext().frame());
    }
    
    foreach(z, channels)
    {
        int b = 0;
        int xx = x;
        const int c = colourIndex(z);
        float* cOut = out.writable(z) + x;
        const float* END = cOut + (r - x);
        
        ReadGuard lock(m_mutex);
        if (m_enable_aovs && !fbs.empty() && rbs[f].isReady())
            b = rbs[f].getBufferIndex(z);
        
        while (cOut < END)
        {
            if (fbs.empty() || !rbs[f].isReady() ||
                x >= rbs[f].getWidth() ||
                y >= rbs[f].getHeight() || r > rbs[f].getWidth())
            {
                *cOut = 0.0f;
            }
            else
                *cOut = rbs[f].getBufferPix(b, xx, y, c);
            ++cOut;
            ++xx;
        }
    }
}

void Aton::knobs(Knob_Callback f)
{
    // Hidden knobs
    Format_knob(f, &m_fmtp, "formats_knob", "format");
    Bool_knob(f, &m_capturing, "capturing_knob");
    Float_knob(f, &m_cam_fov, "cam_fov_knob", " cFov");
    
    Divider(f, "Snapshots");
    static const char* output_name[] = {"",  0};
    Knob* output_knob = SceneView_knob(f, 0, output_name, "output_knob", "Output");
    if(output_knob)
        m_node->m_outputKnob = output_knob;
    
    if (f.makeKnobs())
    {
        SceneView_KnobI* outputKnob = m_node->m_outputKnob->sceneViewKnob();
        outputKnob->setSelectionMode(SceneView_KnobI::eSelectionModeHighlight);
        outputKnob->setColumnHeader("Store");
    }
    
    SetFlags(f,  Knob::SAVE_MENU );

    // Main knobs
    Newline(f);
    Button(f, "clear_knob", "Clear");
    Button(f, "clear_all_knob", "Clear All");
    
    Divider(f, "Region Handle");
    Knob* region_knob = BBox_knob(f, m_cropBox, "Area");
    Button(f, "copy_clipboard", "Copy");

    Divider(f, "Write to Files");
    Knob* write_aovs_knob = Bool_knob(f, &m_all_frames, "write_aovs_knob", "Write AOVs");
    Knob* write_multi_frame_knob = Bool_knob(f, &m_all_frames, "write_multi_frame_knob", "Write Multiple Frames");
    Knob* path_knob = File_knob(f, &m_path, "path_knob", "Path");

    Newline(f);
    Button(f, "capture_knob", "Render");
    Button(f, "import_latest_knob", "Read Latest");
    Button(f, "import_all_knob", "Read All");
    
    for (int i=0; i<16; i++)
    {
        std::string knob_name = (boost::format("cM%s")%i).str();
        Float_knob(f, &m_cam_matrix, knob_name.c_str(), knob_name.c_str());
    }

    // Status Bar knobs
    BeginToolbar(f, "toolbar");
    Int_knob(f, &m_port, "port_number", "Port");
    Bool_knob(f, &m_enable_aovs, "enable_aovs_knob", "Read AOVs");
    Bool_knob(f, &m_multiframes, "multi_frame_knob", "Read Multiple Frames");
    Knob* live_cam_knob = Bool_knob(f, &m_live_camera, "live_camera_knob", "Read Camera");
    EndToolbar(f);
    
    // Status Bar
    BeginToolbar(f, "status_bar");
    Knob* statusKnob = String_knob(f, &m_status, "status_knob", "");
    EndToolbar(f);

    // Set Flags
    path_knob->set_flag(Knob::NO_RERENDER, true);
    live_cam_knob->set_flag(Knob::NO_RERENDER, true);
    write_multi_frame_knob->set_flag(Knob::NO_RERENDER, true);
    statusKnob->set_flag(Knob::NO_RERENDER, true);
    statusKnob->set_flag(Knob::DISABLED, true);
    statusKnob->set_flag(Knob::OUTPUT_ONLY, true);
}

int Aton::knob_changed(Knob* _knob)
{
    if (_knob->is("output_knob"))
    {
        flagForUpdate();
        return 1;
    }
    
    if (_knob->is("port_number"))
    {
        changePort(m_port);
        return 1;
    }
    if (_knob->is("clear_all_knob"))
    {
        clearAllCmd();
        return 1;
    }
    if (_knob->is("multi_frame_knob"))
    {
        if (!m_node->m_framebuffers.empty())
        {
            FrameBuffer& fb = current_framebuffer();
            fb.current_frame(uiContext().frame());
        }
        return 1;
    }
    if (_knob->is("live_camera_knob"))
    {
        liveCameraToogle();
        return 1;
    }
    if (_knob->is("capture_knob"))
    {
        captureCmd();
        return 1;
    }
    if (_knob->is("import_latest_knob"))
    {
        importCmd(false);
        return 1;
    }
    if (_knob->is("import_all_knob"))
    {
        importCmd(true);
        return 1;
    }
    return 0;
}

void Aton::resetChannels(ChannelSet& channels)
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

bool Aton::isPathValid(std::string path)
{
    boost::filesystem::path filepath(path);
    boost::filesystem::path dir = filepath.parent_path();
    return boost::filesystem::exists(dir);
}

std::string Aton::getPath()
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

int Aton::getPort()
{
    const char* def_port = getenv("ATON_PORT");
    int aton_port;
    
    if (def_port == NULL)
        aton_port = 9201;
    else
        aton_port = atoi(def_port);
    
    return aton_port;
}

std::string Aton::getDateTime()
{
    // Returns date and time
    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[20];

    time (&rawtime);
    timeinfo = localtime(&rawtime);

    // Setting up the Date and Time format style
    strftime(time_buffer, 20, "%Y-%m-%d_%H-%M-%S", timeinfo);

    return std::string(time_buffer);
}

std::vector<std::string> Aton::getCaptures()
{
    // Our captured filenames list
    std::vector<std::string> results;
    
    // If the directory exist
    if (isPathValid(m_path))
    {
        using namespace boost::filesystem;
        path filepath(m_path);
        directory_iterator it(filepath.parent_path());
        directory_iterator end;

        // Regex expression to find captured files
        std::string exp = ( boost::format("%s.+.%s")%filepath.stem().string()
                                                    %filepath.extension().string() ).str();
        const boost::regex filter(exp);

        // Iterating through directory to find matching files
        BOOST_FOREACH(path const& p, std::make_pair(it, end))
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

void Aton::clearAllCmd()
{
    std::vector<std::string>& output = m_node->m_output;
    std::vector<FrameBuffer>& fBs = m_node->m_framebuffers;
    
    if (!fBs.empty() && !output.empty())
    {
        
        m_node->m_legit = false;
        m_node->disconnect();
        
        fBs =  std::vector<FrameBuffer>();
        output = std::vector<std::string>();
        
        resetChannels(m_node->m_channels);
        m_node->m_legit = true;
        
        flagForUpdate();
        setStatus();
    }
}

void Aton::captureCmd()
{
    std::string path = std::string(m_path);

    if (m_node->m_frames.size() > 0 && isPathValid(path) && m_slimit > 0)
    {
        // Add date or frame suffix to the path
        std::string key (".");
        std::string timeFrameSuffix;
        std::string frames;
        double startFrame;
        double endFrame;
        
        std::vector<double> sortedFrames = m_node->m_frames;
        std::stable_sort(sortedFrames.begin(), sortedFrames.end());

        if (m_multiframes && m_all_frames)
        {
            timeFrameSuffix += "_" + std::string("####");
            startFrame = sortedFrames.front();
            endFrame = sortedFrames.back();
            
            std::vector<double>::iterator it;
            for(it = sortedFrames.begin(); it != sortedFrames.end(); ++it)
                frames += (boost::format("%s,")%*it).str();
            
            frames.resize(frames.size() - 1);
        }
        else
        {
            timeFrameSuffix += "_" + getDateTime();
            startFrame = endFrame = uiContext().frame();
            frames = (boost::format("%s")%uiContext().frame()).str();
        }

        timeFrameSuffix += ".";
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
                                         "def status(b):\n\t\t"
                                             "nuke.toNode('%s')['capturing_knob'].setValue(b)\n\t\t"
                                             "if not b:\n\t\t\t"
                                                 "nuke.delete(nuke.toNode('%s'))\n\t"
                                         "nuke.executeInMainThread(status, args=True)\n\t"
                                         "nuke.executeInMainThread(nuke.execute, args=('%s', nuke.FrameRanges([%s])))\n\t"
                                         "nuke.executeInMainThread(status, args=False)\n"
                                     "thread.start_new_thread(writer,())''')")%m_node->m_node_name
                                                                              %writeNodeName
                                                                              %writeNodeName
                                                                              %writeNodeName
                                                                              %frames).str();
        script_command(cmd.c_str(), true, false);
        script_unlock();
    }
}

void Aton::importCmd(bool all)
{
    std::vector<std::string> captures = getCaptures();
    if (!captures.empty())
    {
        using namespace boost::filesystem;
        path filepath(m_path);
        path dir = filepath.parent_path();
        
        // Reverse iterating through vector
        std::vector<std::string>::reverse_iterator it;
        for(it = captures.rbegin(); it != captures.rend(); ++it)
        {
            if (all == false && it != captures.rbegin())
                continue;

            path file = *it;
            path path = dir / file;
            std::string str_path = path.string();
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

void Aton::liveCameraToogle()
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

void Aton::setStatus(const long long& progress,
                     const long long& ram,
                     const long long& p_ram,
                     const int& time,
                     const double& frame,
                     const char* version,
                     const char* samples)
{
    const int hour = time / 3600000;
    const int minute = (time % 3600000) / 60000;
    const int second = ((time % 3600000) % 60000) / 1000;
    
    FrameBuffer& fb = current_framebuffer();
    size_t f_count = 0;
    if (!m_node->m_framebuffers.empty())
        f_count = fb.get_frame_count();

    std::string str_status = (boost::format("Arnold %s | "
                                            "Memory: %sMB / %sMB | "
                                            "Time: %02ih:%02im:%02is | "
                                            "Frame: %s(%s) | "
                                            "Samples: %s | "
                                            "Progress: %s%%")%version%ram%p_ram
                                                             %hour%minute%second
                                                             %frame%f_count%samples%progress).str();
    knob("status_knob")->set_text(str_status.c_str());
}

void Aton::setCameraKnobs(const float& fov, const Matrix4& matrix)
{
    std::string knob_value = (boost::format("%s")%fov).str();
    knob("cam_fov_knob")->set_text(knob_value.c_str());
    
    int k_index = 0;
    for (int i=0; i<4; i++)
    {
        for (int j=0; j<4; j++)
        {
            const float value_m = *(matrix[i]+j);
            knob_value = (boost::format("%s")%value_m).str();
            std::string knob_name = (boost::format("cM%s")%k_index).str();
            knob(knob_name.c_str())->set_text(knob_value.c_str());
            k_index++;
        }
    }
}

void Aton::setCurrentFrame(const double& frame)
{
    // Set Current Frame and update the UI
    if (frame != uiContext().frame())
    {
        OutputContext ctxt = outputContext();
        ReadGuard lock(m_mutex);
        ctxt.setFrame(frame);
        gotoContext(ctxt, true);
    }
}

// Nuke node builder
static Iop* constructor(Node* node){ return new Aton(node); }
const Iop::Description Aton::desc(CLASS, 0, constructor);
