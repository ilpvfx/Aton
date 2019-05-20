/*
Copyright (c) 2019,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef Aton_h
#define Aton_h

#include <DDImage/Iop.h>
#include <DDImage/Row.h>
#include <DDImage/Knobs.h>
#include <DDImage/Thread.h>
#include <DDImage/Version.h>
#include <DDImage/TableKnobI.h>

using namespace DD::Image;

#include "aton_client.h"
#include "aton_server.h"
#include "aton_framebuffer.h"

// Class name
static const char* const CLASS = "Aton";

// Help
static const char* const HELP =
    "Aton v1.3.5 \n"
    "Listens for renders coming from the Aton display driver. "
    "For more info go to http://sosoyan.github.io/Aton/";

std::string get_date()
{
    // Returns date and time
    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[15];
    
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    
    // Setting up the Date and Time format style
    strftime(time_buffer, 20, "%m.%d_%H:%M:%S", timeinfo);
    
    return std::string(time_buffer);
}

// Nuke node
class Aton: public Iop
{
    public:
        Aton*                     m_node;               // First node pointer
        Server                    m_server;             // Aton::Server
        ReadWriteLock             m_mutex;              // Mutex for locking the pixel buffer
        Format                    m_fmt;                // The nuke display format
        FormatPair                m_fmtp;               // Buffer format (knob)
        ChannelSet                m_channels;           // Channels aka AOVs object
        int                       m_port;               // Port we're listening on (knob)
        int                       m_output_changed;     // If Snapshots needs to be updated
        float                     m_cam_fov;            // Default Camera fov
        float                     m_cam_matrix;         // Default Camera matrix value
        bool                      m_multiframes;        // Enable Multiple Frames toogle
        bool                      m_write_frames;       // Write AOVs
        bool                      m_enable_aovs;        // Enable AOVs toogle
        bool                      m_live_camera;        // Enable Live Camera toogle
        bool                      m_inError;            // Error handling
        bool                      m_format_exists;      // If the format was already exist
        bool                      m_capturing;          // Capturing signal
        bool                      m_legit;              // Used to throw the threads
        bool                      m_running;            // Thread Rendering
        unsigned int              m_hash_count;         // Refresh hash counter
        const char*               m_path;               // Default path for Write node
        double                    m_region[4];          // Render Region Data
        std::string               m_node_name;          // Node name
        std::string               m_status;             // Status bar text
        std::string               m_connection_error;   // Connection error report
        Knob*                     m_outputKnob;         // Shapshots Knob
        std::vector<FrameBuffer>  m_framebuffers;       // Framebuffers List

        Aton(Node* node): Iop(node),
                          m_node(first_node()),
                          m_fmt(Format(0, 0, 1.0)),
                          m_channels(Mask_RGBA),
                          m_port(get_port()),
                          m_cam_fov(0),
                          m_cam_matrix(0),
                          m_output_changed(0),
                          m_multiframes(false),
                          m_enable_aovs(true),
                          m_live_camera(false),
                          m_write_frames(false),
                          m_inError(false),
                          m_format_exists(false),
                          m_capturing(false),
                          m_legit(false),
                          m_running(false),
                          m_path(""),
                          m_node_name(""),
                          m_status(""),
                          m_connection_error("")
        {
            inputs(0);
            m_region[0] = m_region[1] = m_region[2] =  m_region[3] = 0.0f;
        }

        ~Aton() { disconnect(); }
        
        Aton* first_node() { return dynamic_cast<Aton*>(firstOp()); }
    
        void attach();
        void detach();
        void append(Hash& hash);

        void _validate(bool for_real);
        void engine(int y, int x, int r, ChannelMask channels, Row& out);
        void knobs(Knob_Callback f);
        int knob_changed(Knob* _knob);
    
        enum KnobChanged
        {
            item_not_changed = 0,
            item_added,
            item_copied,
            item_moved_up,
            item_moved_down,
            item_removed
        };
    
        int get_port();
        std::string get_path();
    
        void disconnect();
        void change_port(int port);
        void flag_update(const Box& box = Box(0,0,0,0));

        FrameBuffer* add_framebuffer();
        FrameBuffer* current_framebuffer();
        FrameBuffer* get_framebuffer(const long long& session);
        RenderBuffer* current_renderbuffer();

        int current_fb_index(bool direction = true);
        std::vector<int> selected_fb_indexes();
    
        void set_outputs();
        void set_format(const int& width,
                        const int& height,
                        const float& pixel_aspect);
        void set_channels(std::vector<std::string>& aovs,
                          const bool& ready);
        void reset_channels(ChannelSet& channels);
        void set_camera(const float& fov,
                        const Matrix4& matrix);
        void set_current_frame(const double& frame);
        void set_status(const long long& progress = 0,
                        const long long& ram = 0,
                        const long long& p_ram = 0,
                        const int& time = 0,
                        const double& frame = 0,
                        const char* version = "",
                        const char* samples = "",
                        const char* output = "");
    
        void live_camera_toogle();
        bool path_valid(std::string path);
        std::vector<std::string> get_captures();
    
        void multiframe_cmd();
        void select_output_cmd();
        void snapshot_cmd();
        void move_cmd(bool direction);
        void remove_selected_cmd();
        void reset_region_cmd();
        void copy_region_cmd();
        void capture_cmd();
        void import_cmd(bool all);
    
        bool firstEngineRendersWholeRequest() const { return true; }
        const char* Class() const { return CLASS; }
        const char* displayName() const { return CLASS; }
        const char* node_help() const { return HELP; }
        static const Iop::Description desc;
};

#endif /* Aton_h */
