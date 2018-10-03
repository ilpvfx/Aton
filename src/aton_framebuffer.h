/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FenderBuffer_h
#define FenderBuffer_h

#include <DDImage/Iop.h>
#include "aton_client.h"

using namespace DD::Image;

std::string get_date();

namespace chStr
{
    extern const std::string RGBA, rgb, depth, Z, N, P, ID,
                             _red, _green, _blue, _X, _Y, _Z;
}

// Unpack 1 int to 4
const std::vector<int> unpack_4_int(const int& i);

// Lightweight colour pixel class
class RenderColor
{
public:
    RenderColor();
    
    float& operator[](int i);
    const float& operator[](int i) const;
    
    void reset();
    
    // Data
    float _val[3];
};

// AOV Buffer class
class AOVBuffer
{
    friend class RenderBuffer;
    
public:
    AOVBuffer(const unsigned int& width = 0,
              const unsigned int& height = 0,
              const int& spp = 0);
    
private:
    // Data
    std::vector<RenderColor> _color_data;
    std::vector<float> _float_data;
};


// RenderBuffer main class
class RenderBuffer
{
    friend class FrameBuffer;
    
public:
    RenderBuffer(const double& currentFrame = 0,
                 const int& w = 0,
                 const int& h = 0,
                 const float& _pix_aspect = 1.0f);
    
    // Add new buffer
    void add_aov(const char* aov = NULL,
                 const int& spp = 0);
    
    // Set writable buffer's pixel
    void set_aov_pix(const int& b,
                     const int& x,
                     const int& y,
                     const int& spp,
                     const int& c,
                     const float& pix);
    
    // Get read only buffer's pixel
    const float& get_aov_pix(const int& b,
                             const int& x,
                             const int& y,
                             const int& c) const;
    
    // Get AOVs
    std::vector<std::string>& get_aovs() { return _aovs; }
    
    // Get the current buffer index
    int get_aov_index(const Channel& z);
    
    // Get the current buffer index
    int get_aov_index(const char* aovName);
    
    // Get N buffer/aov name name
    std::string get_aov_name(const int& index);
    
    // Get last buffer/aov name
    bool first_aov_name(const char* aovName);
    
    // Check if Frame has been changed
    bool frame_changed(const double& frame) { return frame != _frame; }
    
    bool name_changed(const char* name) { return strcmp(name, _name.c_str()) != 0; }
    
    // Check if Aovs have been changed
    bool aovs_changed(const std::vector<std::string>& aovs);
    
    // Check if Resolution has been changed
    bool resolution_changed(const unsigned int& w,
                            const unsigned int& h);
    
    // Check if Camera fov has been changed
    bool camera_changed(const float& fov, const Matrix4& matrix);
    
    // Resize the containers to match the resolution
    void set_resolution(const unsigned int& w,
                        const unsigned int& h);
    
    // Clear buffers and aovs
    void clear_all();
    
    // Check if the given buffer/aov name name is exist
    bool aov_exists(const char* aovName);
    
    // Get width of the buffer
    const int& get_width() const { return _width; }
    
    // Get height of the buffer
    const int& get_height() const { return _height; }
    
    // Get pixel aspect of the buffer
    const float& get_pixel_aspect() const { return _pix_aspect; }
    
    // Check if this RenderBuffer is empty
    bool empty() { return _aovs.empty(); }
    
    // Get size of the buffers aka AOVs count
    size_t size() { return _aovs.size(); }
    
    // Resize the buffers
    void resize(const size_t& s);
    
    // Status parameters
    const long long& get_progress() { return _progress; }
    void set_progress(const long long& progress = 0);
    
    const long long& get_memory() { return _ram; }
    const long long& get_peak_memory() { return _pram; }
    void set_memory(const long long& ram = 0);
    
    const int& get_time() { return _time; }
    void set_time(const int& time = 0,
                  const int& dtime = 0);
    
    // Arnold core version
    const int& get_version_int() { return _version_int; }
    const char* get_version_str() { return _version_str.c_str(); }
    void set_version(const int& version);

    // Sampling
    const std::vector<int> get_samples_int() { return _samples; }
    const char* get_samples() { return _samples_str.c_str(); }
    void set_samples(const std::vector<int> samples);

    // Get/Set the frame number of this RenderBuffer
    const double& get_frame() { return _frame; }
    void set_frame(const double& frame) { _frame = frame; }
    
    // To keep False while writing the buffer
    void set_ready(const bool& ready) { _ready = ready; }
    const bool& ready() const { return _ready; }
    
    // Camera
    const float& get_camera_fov() { return _fov; }
    const Matrix4& get_camera_matrix() { return _matrix; }
    void set_camera(const float& fov, const Matrix4& matrix);
    
    // Name
    const char* get_name() { return _name.c_str(); }
    void set_name(std::string name) { _name = name; }
    
private:
    double _frame;
    long long _progress;
    int _time;
    long long _ram;
    long long _pram;
    int _width;
    int _height;
    float _pix_aspect;
    bool _ready;
    float _fov;
    Matrix4 _matrix;
    int _version_int;
    std::string _name;
    std::vector<int> _samples;
    std::string _version_str;
    std::string _samples_str;
    std::vector<AOVBuffer> _buffers;
    std::vector<std::string> _aovs;
};

// FrameBuffer Class
class FrameBuffer
{
public:
    FrameBuffer() {};
    
    RenderBuffer* get_renderbuffer(double frame);
    
    RenderBuffer* current_renderbuffer() { return get_renderbuffer(_frame); }
    
    std::vector<RenderBuffer>& get_renderbuffers() { return _renderbuffers; }
    
    const std::vector<double>& frames() { return _frames; }
    
    size_t size() { return _frames.size(); }
    
    // Add New RenderBuffer
    RenderBuffer* add_renderbuffer(DataHeader* dh);
    
    // Update RenderBuffer
    void update_renderbuffer(DataHeader* dh);
    
    // Check if RenderBuffer already exists
    bool renderbuffer_exists(double frame);
    
    double get_frame() { return _frame; }
    void set_frame(double frame) { _frame = frame; }
    
    long long& get_session() { return _session; }
    void set_session(long long session) { _session = session; }
    
    std::string get_output_name() { return _output_name; }
    void set_output_name(std::string name) { _output_name = name; }
    
    // Clear All Data
    void clear_all();
    bool empty() { return _renderbuffers.empty(); }

private:
    double _frame;
    long long _session;
    std::string _output_name;
    std::vector<double> _frames;
    std::vector<RenderBuffer> _renderbuffers;
};

#endif /* FenderBuffer_h */
