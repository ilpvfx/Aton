/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/


#include "aton_framebuffer.h"
#include "boost/format.hpp"
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
                                            _ready(false) {}
// Add new buffer
void RenderBuffer::addBuffer(const char* aov,
                            const int& spp)
{
    AOVBuffer buffer(_width, _height, spp);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get writable buffer object
void RenderBuffer::setBufferPix(const int& b,
                                const unsigned int& x,
                                const unsigned int& y,
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
const float& RenderBuffer::getBufferPix(const int& b,
                                        const unsigned int& x,
                                        const unsigned int& y,
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
int RenderBuffer::getBufferIndex(const Channel& z)
{
    int b_index = 0;
    if (_aovs.size() > 1)
    {
        using namespace chStr;
        const std::string& layer = getLayerName(z);

        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == layer)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
            else if (*it == Z && layer == depth)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }

    }
    return b_index;
}

// Get the current buffer index
int RenderBuffer::aovIndex(const char* aovName)
{
    int b_index = 0;
    if (_aovs.size() > 1)
    {
        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == aovName)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }
    }
    return b_index;
}

// Get N buffer/aov name name
const char* RenderBuffer::getBufferName(const int& index)
{
    const char* aovName = "";

    if(!_aovs.empty())
    {
        try
        {
            aovName = _aovs.at(index).c_str();
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
    
    return aovName;
}

// Get last buffer/aov name
bool RenderBuffer::isFirstBufferName(const char* aovName)
{
    return strcmp(_aovs.front().c_str(), aovName) == 0;;
}

// Check if Aovs has been changed
bool RenderBuffer::isAovsChanged(const std::vector<std::string>& aovs)
{
    return (aovs != _aovs);
}

// Check if Resolution has been changed
bool RenderBuffer::isResolutionChanged(const unsigned int& w,
                                       const unsigned int& h)
{
    return (w != _width || h != _height);
}

bool RenderBuffer::isCameraChanged(const float& fov,
                                  const Matrix4& matrix)
{
    return (_fov != fov || _matrix != matrix);
}

// Resize the containers to match the resolution
void RenderBuffer::setResolution(const unsigned int& w,
                                 const unsigned int& h)
{
    _width = w;
    _height = h;
    
    const int bfSize = _width * _height;
    
    std::vector<AOVBuffer>::iterator iRB;
    for(iRB = _buffers.begin(); iRB != _buffers.end(); ++iRB)
    {
        if (!iRB->_color_data.empty())
        {
            RenderColor color;
            std::fill(iRB->_color_data.begin(), iRB->_color_data.end(), color);
            iRB->_color_data.resize(bfSize);
        }
        if (!iRB->_float_data.empty())
        {
            std::fill(iRB->_float_data.begin(), iRB->_float_data.end(), 0.0f);
            iRB->_float_data.resize(bfSize);
        }
    }
}

// Clear buffers and aovs
void RenderBuffer::clearAll()
{
    _buffers = std::vector<AOVBuffer>();
    _aovs = std::vector<std::string>();
}

// Check if the given buffer/aov name name is exist
bool RenderBuffer::aovExists(const char* aovName)
{
    return std::find(_aovs.begin(), _aovs.end(), aovName) != _aovs.end();
}

// Resize the buffers
void RenderBuffer::resize(const size_t& s)
{
    _buffers.resize(s);
    _aovs.resize(s);
}

// Set status parameters
void RenderBuffer::setProgress(const long long& progress)
{
    _progress = progress > 100 ? 100 : progress;
}

void RenderBuffer::setRAM(const long long& ram)
{
    const int ramGb = static_cast<int>(ram / 1048576);
    _ram = ramGb;
    _pram = ramGb > _pram ? ramGb : _pram;

}
void RenderBuffer::setTime(const int& time,
                          const int& dtime)
{
    _time = dtime > time ? time : time - dtime;
}

// Set Version
void RenderBuffer::setVersion(const int& version)
{
    const std::vector<int> ver = unpack_4_int(version);

    _versionStr = lexical_cast<string>(ver[0]) + "." +
                  lexical_cast<string>(ver[1]) + "." +
                  lexical_cast<string>(ver[2]) + "." +
                  lexical_cast<string>(ver[3]);
}

// Set Samples
void RenderBuffer::setSamples(std::vector<int> sp)
{
    _samplesStr = lexical_cast<string>(sp[0]) + "/" +
                  lexical_cast<string>(sp[1]) + "/" +
                  lexical_cast<string>(sp[2]) + "/" +
                  lexical_cast<string>(sp[3]) + "/" +
                  lexical_cast<string>(sp[4]) + "/" +
                  lexical_cast<string>(sp[5]);
}


void RenderBuffer::setCamera(const float& fov, const Matrix4& matrix)
{
    _fov = fov;
    _matrix = matrix;
}


RenderBuffer& FrameBuffer::getFrame(double frame)
{
    return _renderbuffers[getIndex(frame)];
}

void FrameBuffer::addFrame(double frame, int xres, int yres, float pix_aspect)
{
        RenderBuffer rb(frame, xres, yres, pix_aspect);
        if (!_frames.empty())
            rb = _renderbuffers.back();
        
        _frames.push_back(frame);
        _renderbuffers.push_back(rb);
        _current_frame  = frame;
}

// Clear All Data
void FrameBuffer::clearAll()
{
    _frames = std::vector<double>();
    _renderbuffers = std::vector<RenderBuffer>();
}

void FrameBuffer::clearAllExcept(double frame)
{
    std::swap(_frames.at(0), _frames.at(getIndex(frame)));
    std::swap(_renderbuffers.at(0), _renderbuffers.at(getIndex(frame)));

    _frames.erase(_frames.begin() + 1, _frames.end());
    _renderbuffers.erase(_renderbuffers.begin() + 1, _renderbuffers.end());
    _current_frame  = frame;
}


// Check if RenderBuffer already exists
bool FrameBuffer::frameExists(double frame)
{
    if (!_frames.empty())
        return (std::find(_frames.begin(), _frames.end(), frame) != _frames.end());
    else
        return false;
}

// Get RenderBuffer for given Frame
int FrameBuffer::getIndex(double frame)
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
    return index;
}


