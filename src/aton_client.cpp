/*
Copyright (c) 2019,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include "aton_client.h"
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::asio;

const int get_port()
{
    const char* def_port = getenv("ATON_PORT");
    int aton_port;
    
    if (def_port == NULL)
        aton_port = 9201;
    else
        aton_port = atoi(def_port);
    
    return aton_port;
}

const std::string get_host()
{
    const char* def_host = getenv("ATON_HOST");
    
    if (def_host == NULL)
        def_host = "127.0.0.1";

    std::string aton_host = def_host;
    return aton_host;
}

const bool host_exists(const char* host)
{
    boost::system::error_code ec;
    ip::address::from_string(host, ec);
    return !ec;
}

const long long get_unique_id()
{
    using namespace boost::posix_time;
    ptime time_t_epoch(boost::gregorian::date(1970,1,1));
    ptime now = microsec_clock::local_time();
    time_duration diff = now - time_t_epoch;
    return diff.total_milliseconds();
}

const int pack_4_int(int a, int b, int c, int d)
{
    return a * 1000000 + b * 10000 + c * 100 + d;
}

// Data Class
DataHeader::DataHeader(const long long& index,
                       const int& xres,
                       const int& yres,
                       const float& pix_aspect,
                       const long long& region_area,
                       const int& version,
                       const float& frame,
                       const float& cam_fov,
                       const float* cam_matrix,
                       const int* samples,
                       const char* output_name): mSession(index),
                                                 mXres(xres),
                                                 mYres(yres),
                                                 mPixAspectRatio(pix_aspect),
                                                 mRArea(region_area),
                                                 mVersion(version),
                                                 mFrame(frame),
                                                 mCamFov(cam_fov),
                                                 mOutputName(output_name)
{
    if (cam_matrix != NULL)
        mCamMatrix = const_cast<float*>(cam_matrix);
    
    if (samples != NULL)
        mSamples = const_cast<int*>(samples);
}

DataHeader::~DataHeader() {}

void DataHeader::free()
{
    delete[] mOutputName;
    mOutputName = NULL;
}

DataPixels::DataPixels(const long long& session,
                       const int& xres,
                       const int& yres,
                       const int& bucket_xo,
                       const int& bucket_yo,
                       const int& bucket_size_x,
                       const int& bucket_size_y,
                       const int& spp,
                       const long long& ram,
                       const int& time,
                       const char* aovName,
                       const float* data) : mSession(session),
                                            mXres(xres),
                                            mYres(yres),
                                            mBucket_xo(bucket_xo),
                                            mBucket_yo(bucket_yo),
                                            mBucket_size_x(bucket_size_x),
                                            mBucket_size_y(bucket_size_y),
                                            mSpp(spp),
                                            mRam(ram),
                                            mTime(time),
                                            mAovName(aovName)

{
    if (data != NULL)
        mpData = const_cast<float*>(data);
}

DataPixels::~DataPixels() {}

void DataPixels::free()
{
    delete[] mAovName;
    mAovName = NULL;
}


// Client Class
Client::Client(std::string hostname, int port): mHost(hostname),
                                                mPort(port),
                                                mImageId(-1),
                                                mSocket(mIoService),
                                                mIsConnected(false) {}

Client::~Client()
{
    disconnect();
}

void Client::connect()
{
    using boost::asio::ip::tcp;
    tcp::resolver resolver(mIoService);
    tcp::resolver::query query(mHost.c_str(), boost::lexical_cast<std::string>(mPort).c_str());
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end)
    {
        mSocket.close();
        mSocket.connect(*endpoint_iterator++, error);
    }
    if (error)
        throw boost::system::system_error(error);
}

void Client::disconnect()
{
    mSocket.close();
}

void Client::send_header(DataHeader& header)
{
    // Connect to port!
    connect();

    // Send image header message with image desc information
    int key = 0;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));
    
    // Send our width & height
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mSession), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mXres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mYres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mPixAspectRatio), sizeof(float)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mRArea), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mVersion), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mFrame), sizeof(float)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mCamFov), sizeof(float)));

    const int camMatrixSize = 16;
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mCamMatrix[0]), sizeof(float)*camMatrixSize));
    
    const int samplesSize = 6;
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mSamples[0]), sizeof(int)*samplesSize));
    
    // Get size of aov name
    size_t output_size = strlen(header.mOutputName) + 1;
    write(mSocket, buffer(reinterpret_cast<char*>(&output_size), sizeof(size_t)));
    write(mSocket, buffer(header.mOutputName, output_size));
    mIsConnected = true;
}

void Client::send_pixels(DataPixels& pixels, bool reconnect)
{
    // Reconnect to port!
    if (reconnect)
        connect();
    
    // Send data for image_id
    int key = 1;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));

    // Get size of aov name
    size_t aov_size = strlen(pixels.mAovName) + 1;

    // Get size of overall samples
    const int num_samples = pixels.mBucket_size_x * pixels.mBucket_size_y * pixels.mSpp;
    
    // Sending data to buffer
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mSession), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mXres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mYres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_xo), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_yo), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_size_x), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_size_y), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mSpp), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mRam), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mTime), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&aov_size), sizeof(size_t)));
    write(mSocket, buffer(pixels.mAovName, aov_size));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mpData[0]), sizeof(float)*num_samples));
}

void Client::close_image()
{
    // Send image complete message for image_id
    int key = 2;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));

    // Disconnect from port!
    disconnect();
}

void Client::quit()
{
    connect();
    int key = 9;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));
    disconnect();
}
