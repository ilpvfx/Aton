/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include "aton_framebuffer.h"
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

const std::string chStr::RGBA = "RGBA",
                  chStr::rgb = "rgb",
                  chStr::depth = "depth",
                  chStr::Z = "Z",
                  chStr::N = "N",
                  chStr::P = "P",
                  chStr::ID = "ID",
                  chStr::_red = ".red",
                  chStr::_green = ".green",
                  chStr::_blue = ".blue",
                  chStr::_X = ".X",
                  chStr::_Y = ".Y",
                  chStr::_Z = ".Z";

// Unpack 1 int to 4
const std::vector<int> unpack_4_int(const int& i)
{
    const int arr[] = {((i % 10000000) / 1000000),
                       ((i % 1000000) / 10000),
                       ((i % 10000) / 100),
                        (i % 100)};
    
    std::vector<int> out(arr, arr + 4);
    return out;
}


// Lightweight color pixel class
RenderColor::RenderColor() { _val[0] = _val[1] = _val[2] = 0.0f; }

float& RenderColor::operator[](int i) { return _val[i]; }

const float& RenderColor::operator[](int i) const { return _val[i]; }

void RenderColor::reset() { _val[0] = _val[1] = _val[2] = 0.0f; }


// AOVBuffer class
AOVBuffer::AOVBuffer(const unsigned int& width,
                     const unsigned int& height,
                     const int& spp)
{
    const int size = width * height;
    
    switch (spp)
    {
        case 1: // Float channels
            _float_data.resize(size);
            break;
        case 3: // Color Channels
            _color_data.resize(size);
            break;
        case 4: // Color + Alpha channels
            _color_data.resize(size);
            _float_data.resize(size);
            break;
    }
}

// RenderBuffer class
RenderBuffer::RenderBuffer(const double& currentFrame,
                           const int& w,
                           const int& h,
                           const float& p): _frame(currentFrame),
                                            _width(w),
                                            _height(h),
                                            _pix_aspect(p),
                                            _progress(0),
                                            _time(0),
                                            _ram(0),
                                            _pram(0),
                                            _ready(false),
                                            _fov(0.0f),
                                            _matrix(Matrix4()),
                                            _version_int(0),
                                            _version_str(""),
                                            _samples_str("") {}
// Add new buffer
void RenderBuffer::add_aov(const char* aov,
                           const int& spp)
{
    AOVBuffer buffer(_width, _height, spp);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get writable buffer object
void RenderBuffer::set_aov_pix(const int& b,
                               const int& x,
                               const int& y,
                               const int& spp,
                               const int& c,
                               const float& pix)
{
    AOVBuffer& rb = _buffers[b];
    const unsigned int index = (_width * y) + x;
    if (c < 3 && spp != 1)
        rb._color_data[index][c] = pix;
    else
        rb._float_data[index] = pix;
}

// Get read only buffer object
const float& RenderBuffer::get_aov_pix(const int& b,
                                       const int& x,
                                       const int& y,
                                       const int& c) const
{
    const AOVBuffer& rb = _buffers[b];
    const unsigned int index = (_width * y) + x;
    if (c < 3 && !rb._color_data.empty())
        return rb._color_data[index][c];
    else
        return rb._float_data[index];
}

// Get the current buffer index
int RenderBuffer::get_aov_index(const Channel& z)
{
    int aov_index = 0;
    if (_aovs.size() > 1)
    {
        using namespace chStr;
        const std::string& layer = getLayerName(z);

        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == layer)
            {
                aov_index = static_cast<int>(it - _aovs.begin());
                break;
            }
            else if (*it == Z && layer == depth)
            {
                aov_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }

    }
    return aov_index;
}

// Get the current buffer index
int RenderBuffer::get_aov_index(const char* aov_name)
{
    int aov_index = 0;
    if (_aovs.size() > 1)
    {
        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == aov_name)
            {
                aov_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }
    }
    return aov_index;
}

// Get N buffer/aov name name
std::string RenderBuffer::get_aov_name(const int& aov_index)
{
    std::string aov_name = "";

    if(!_aovs.empty())
    {
        try
        {
            aov_name = _aovs.at(aov_index);
        }
        catch (const std::out_of_range& e)
        {
            (void)e;
        }
        catch (...)
        {
            std::cerr << "Unexpected error at getting buffer name" << std::endl;
        }
    }
    
    return aov_name;
}

// Get last buffer/aov name
bool RenderBuffer::first_aov_name(const char* aovName)
{
    return strcmp(_aovs.front().c_str(), aovName) == 0;
}

// Check if Aovs has been changed
bool RenderBuffer::aovs_changed(const std::vector<std::string>& aovs)
{
    return (aovs != _aovs);
}

// Check if Resolution has been changed
bool RenderBuffer::resolution_changed(const unsigned int& w,
                                      const unsigned int& h)
{
    return (w != _width || h != _height);
}

bool RenderBuffer::camera_changed(const float& fov,
                                  const Matrix4& matrix)
{
    return (_fov != fov || _matrix != matrix);
}

// Resize the containers to match the resolution
void RenderBuffer::set_resolution(const unsigned int& w,
                                  const unsigned int& h)
{
    _width = w;
    _height = h;
    
    const int size = _width * _height;
    
    std::vector<AOVBuffer>::iterator it;
    for(it = _buffers.begin(); it != _buffers.end(); ++it)
    {
        if (!it->_color_data.empty())
        {
            RenderColor color;
            std::fill(it->_color_data.begin(), it->_color_data.end(), color);
            it->_color_data.resize(size);
        }
        if (!it->_float_data.empty())
        {
            std::fill(it->_float_data.begin(), it->_float_data.end(), 0.0f);
            it->_float_data.resize(size);
        }
    }
}

// Clear buffers and aovs
void RenderBuffer::clear_all()
{
    _buffers = std::vector<AOVBuffer>();
    _aovs = std::vector<std::string>();
}

// Check if the given buffer/aov name name is exist
bool RenderBuffer::aov_exists(const char* aovName)
{
    return std::find(_aovs.begin(), _aovs.end(), aovName) != _aovs.end();
}

// Resize the buffers
void RenderBuffer::resize(const size_t& s)
{
    _aovs.resize(s);
    _buffers.resize(s);
}

// Set status parameters
void RenderBuffer::set_progress(const long long& progress)
{
    _progress = progress > 100 ? 100 : progress;
}

void RenderBuffer::set_memory(const long long& ram)
{
    _ram = static_cast<int>(ram / 1048576);
    _pram = _ram > _pram ? _ram : _pram;
}
void RenderBuffer::set_time(const int& time,
                            const int& dtime)
{
    _time = dtime > time ? time : time - dtime;
}

// Set Version
void RenderBuffer::set_version(const int& version)
{
    const std::vector<int> ver = unpack_4_int(version);

    _version_str = lexical_cast<string>(ver[0]) + "." +
                   lexical_cast<string>(ver[1]) + "." +
                   lexical_cast<string>(ver[2]) + "." +
                   lexical_cast<string>(ver[3]);
}

// Set Samples
void RenderBuffer::set_samples(std::vector<int> sp)
{
    _samples_str = lexical_cast<string>(sp[0]) + "/" +
                   lexical_cast<string>(sp[1]) + "/" +
                   lexical_cast<string>(sp[2]) + "/" +
                   lexical_cast<string>(sp[3]) + "/" +
                   lexical_cast<string>(sp[4]) + "/" +
                   lexical_cast<string>(sp[5]);
}


void RenderBuffer::set_camera(const float& fov, const Matrix4& matrix)
{
    _fov = fov;
    _matrix = matrix;
}


RenderBuffer* FrameBuffer::get_renderbuffer(double frame)
{
    int index = 0;
    if (_frames.size() > 1)
    {
        int nearFIndex = INT_MIN;
        int minFIndex = INT_MAX;
        
        std::vector<double>::iterator it;
        for(it = _frames.begin(); it != _frames.end(); ++it)
        {
            if (frame == *it)
            {
                index = static_cast<int>(it - _frames.begin());
                break;
            }
            else if (frame > *it && nearFIndex < *it)
            {
                nearFIndex = static_cast<int>(*it);
                index = static_cast<int>(it - _frames.begin());
                continue;
            }
            else if (*it < minFIndex && nearFIndex == INT_MIN)
            {
                minFIndex = static_cast<int>(*it);
                index = static_cast<int>(it - _frames.begin());
            }
        }
    }
    
    return &_renderbuffers[index];
}

RenderBuffer* FrameBuffer::add_renderbuffer(DataHeader* dh)
{
        RenderBuffer rb(dh->frame(), dh->xres(), dh->yres(), dh->pixel_aspect());
    
        _output_name = (boost::format("%s_%d_%s")%dh->output_name()
                                                 %dh->frame()%get_date()).str();
    
        if (!_frames.empty())
            rb = _renderbuffers.back();

        _frame  = dh->frame();
        _session = dh->session();
        _frames.push_back(dh->frame());
        _renderbuffers.push_back(rb);
        return &_renderbuffers.back();
}

// Udpate RenderBuffer
void FrameBuffer::update_renderbuffer(DataHeader* dh)
{
    _session = dh->session();
    _output_name = (boost::format("%s_%d_%s")%dh->output_name()
                                             %dh->frame()%get_date()).str();
    _frame  = dh->frame();
}

// Clear All Data
void FrameBuffer::clear_all()
{
    _frames = std::vector<double>();
    _renderbuffers = std::vector<RenderBuffer>();
}

// Check if RenderBuffer already exists
bool FrameBuffer::renderbuffer_exists(double frame)
{
    if (!_frames.empty())
        return (std::find(_frames.begin(), _frames.end(), frame) != _frames.end());
    else
        return 0;
}
