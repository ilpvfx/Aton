/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef Aton_h
#define Aton_h

#include "DDImage/Iop.h"
#include "DDImage/Row.h"
#include "DDImage/Knobs.h"
#include "DDImage/Thread.h"
#include "DDImage/Version.h"
#include <DDImage/Enumeration_KnobI.h>
#include <DDImage/TableKnobI.h>


using namespace DD::Image;

#include "aton_client.h"
#include "aton_server.h"
#include "aton_framebuffer.h"

#include <list>

// Class name
static const char* const CLASS = "Aton";

// Help
static const char* const HELP =
    "Aton v1.3.0 \n"
    "Listens for renders coming from the Aton display driver. "
    "For more info go to http://sosoyan.github.io/Aton/";

struct ComboBox
{
    int current_index = 0;
    std::vector<const char*> names;
    std::list<std::string> names_str;
    
    void add(std::string name)
    {
        names_str.push_back(name);
        names.push_back(names_str.back().c_str());
    }
    
    void remove()
    {
        names_str.pop_front();
        names.erase(names.begin());
    }
};


// Nuke node
class Aton: public Iop
{
    public:
        Aton*                     m_node;             // First node pointer
        Server                    m_server;           // Aton::Server
        ReadWriteLock             m_mutex;            // Mutex for locking the pixel buffer
        Format                    m_fmt;              // The nuke display format
        FormatPair                m_fmtp;             // Buffer format (knob)
        ChannelSet                m_channels;         // Channels aka AOVs object
        int                       m_port;             // Port we're listening on (knob)
        int                       m_slimit;           // The limit size
        float                     m_cam_fov;          // Default Camera fov
        float                     m_cam_matrix;       // Default Camera matrix value
        bool                      m_multiframes;      // Enable Multiple Frames toogle
        bool                      m_all_frames;       // Capture All Frames toogle
        bool                      m_stamp;            // Enable Frame stamp toogle
        bool                      m_enable_aovs;      // Enable AOVs toogle
        bool                      m_live_camera;      // Enable Live Camera toogle
        bool                      m_inError;          // Error handling
        bool                      m_formatExists;     // If the format was already exist
        bool                      m_capturing;        // Capturing signal
        bool                      m_legit;            // Used to throw the threads
        double                    m_current_frame;    // Used to hold current frame
        double                    m_stamp_scale;      // Frame stamp size
        unsigned int              m_hash_count;       // Refresh hash counter
        const char*               m_path;             // Default path for Write node
        const char*               m_comment;          // Comment for the frame stamp
        double*                   m_cropBox;
        std::string               m_node_name;        // Node name
        std::string               m_status;           // Status bar text
        std::string               m_details;          // Render layer details
        std::string               m_connectionError;  // Connection error report
        Knob*                     m_outputKnob;
        std::vector<double>       m_frames;           // Frames holder
        std::vector<std::string>  m_output;
        std::vector<FrameBuffer>  m_framebuffers;     // Framebuffers holder
        std::vector<std::string>  m_garbageList;      // List of captured files to be deleted
        RenderBuffer              m_empty_buffer;

        Aton(Node* node): Iop(node),
                          m_node(firstNode()),
                          m_fmt(Format(0, 0, 1.0)),
                          m_channels(Mask_RGBA),
                          m_port(getPort()),
                          m_slimit(20),
                          m_cam_fov(0),
                          m_cam_matrix(0),
                          m_multiframes(true),
                          m_enable_aovs(true),
                          m_live_camera(false),
                          m_all_frames(false),
                          m_stamp(false),
                          m_inError(false),
                          m_formatExists(false),
                          m_capturing(false),
                          m_legit(false),
                          m_cropBox(NULL),
                          m_current_frame(0),
                          m_stamp_scale(1.0),
                          m_path(""),
                          m_node_name(""),
                          m_status(""),
                          m_comment(""),
                          m_connectionError("")
        {
            inputs(0);
        }

        ~Aton() { disconnect(); }
        
        Aton* firstNode() { return dynamic_cast<Aton*>(firstOp()); }
    
        void attach();
        
        void detach();

        void flagForUpdate(const Box& box = Box(0,0,0,0));

        void changePort(int port);

        void disconnect();

        void append(Hash& hash);

        void _validate(bool for_real);

        void engine(int y, int x, int r, ChannelMask channels, Row& out);

        void knobs(Knob_Callback f);

        int knob_changed(Knob* _knob);

        void resetChannels(ChannelSet& channels);
    
        bool isPathValid(std::string path);
    
        int getFrameIndex(std::vector<double>& frames, double currentFrame);
    
        std::string getPath();
    
        int getPort();

        std::string getDateTime();

        std::vector<std::string> getCaptures();
    
        void clearAllCmd();

        void captureCmd();

        void importCmd(bool all);
    
        void liveCameraToogle();
    
        void setStatus(const long long& progress = 0,
                       const long long& ram = 0,
                       const long long& p_ram = 0,
                       const int& time = 0,
                       const double& frame = 0,
                       const char* version = "",
                       const char* samples = "");
    
        void setCameraKnobs(const float& fov, const Matrix4& matrix);
    
        void setCurrentFrame(const double& frame);
    
        bool firstEngineRendersWholeRequest() const { return true; }
        const char* Class() const { return CLASS; }
        const char* displayName() const { return CLASS; }
        const char* node_help() const { return HELP; }
        static const Iop::Description desc;
};

#endif /* Aton_h */
