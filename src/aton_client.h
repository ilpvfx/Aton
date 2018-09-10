/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef ATON_CLIENT_H_
#define ATON_CLIENT_H_

#include <vector>
#include <boost/asio.hpp>


const int get_port();

const std::string get_host();

const bool host_exists(const char* host);

const long long get_unique_id();

const int pack_4_int(int a, int b, int c, int d);


class Client;


class DataHeader
{
    friend class Client;
    friend class Server;
    
public:
    
    DataHeader(const long long& index = 0,
               const int& xres = 0,
               const int& yres = 0,
               const float& pix_aspect = 0.0f,
               const long long& region_area = 0,
               const int& version = 0,
               const float& current_frame = 0.0f,
               const float& cam_fov = 0.0f,
               const float* cam_matrix = NULL,
               const int* samples = NULL,
               const char* outputName = NULL);
    
    ~DataHeader();
    
    // Get Session Index
    const long long& session() const { return mSession; }
    
    // Get x resolution
    const int& xres() const { return mXres; }
    
    // Get y resolution
    const int& yres() const { return mYres; }
    
    // Get Pixel Aspect Ratio
    const float& pixel_aspect() const { return mPixAspectRatio; }
    
    // Get area of the render region
    const long long& region_area() const { return mRArea; }
    
    // Version number
    const int& version() const { return mVersion; }
    
    // Current frame
    const float& frame() const { return mFrame; }
    
    // Camera Fov
    const float& camera_fov() const { return mCamFov; }
    
    // Camera matrix
    const std::vector<float>& camera_matrix() const { return mCamMatrixStore; }
    
    const std::vector<int>& samples() const { return mSamplesStore; }
    
    const char* output_name() const { return mOutputName; }
    
    // Deallocate output name
    void free();

private:
    // Session ID
    long long mSession;
    
    // Resolution, X & Y
    int mXres, mYres;
    
    // Pixel Aspect Ratio
    float mPixAspectRatio;
    
    // Version number
    int mVersion;
    
    // Region area
    long long mRArea;
    
    // Current frame
    float mFrame;
    
    // Camera Field of View
    float mCamFov;
    
    // Camera Matrix pointer, storage
    float* mCamMatrix;
    std::vector<float> mCamMatrixStore;
    
    int* mSamples;
    std::vector<int> mSamplesStore;
    
    // Outout name
    const char *mOutputName;

};


class DataPixels
{
    friend class Client;
    friend class Server;
    
public:
    DataPixels(const int& xres = 0,
               const int& yres = 0,
               const int& bucket_xo = 0,
               const int& bucket_yo = 0,
               const int& bucket_size_x = 0,
               const int& bucket_size_y = 0,
               const int& spp = 0,
               const long long& ram = 0,
               const int& time = 0,
               const char* aovName = NULL,
               const float* data = NULL);
    
    ~DataPixels();
    
    // Get x resolution
    const int& xres() const { return mXres; }
    
    // Get y resolution
    const int& yres() const { return mYres; }
    
    // Get y position
    const int& bucket_xo() const { return mBucket_xo; }
    
    // Get y position
    const int& bucket_yo() const { return mBucket_yo; }
    
    // Get width
    const int& bucket_size_x() const { return mBucket_size_x; }
    
    // Get height
    const int& bucket_size_y() const { return mBucket_size_y; }
    
    // Samples-per-pixel, aka channel depth
    const int& spp() const { return mSpp; }
    
    // Taken memory while rendering
    const long long& ram() const { return mRam; }
    
    // Taken time while rendering
    const unsigned int& time() const { return mTime; }
    
    // Get Aov name
    const char* aov_name() const { return mAovName; }
    
    // Pointer to pixel data owned by the display driver (client-side)
    const float* data() const { return mpData; }
    
    // Reference to pixel data owned by this object (server-side)
    const float& pixel(int index = 0) { return mPixelStore[index]; }
    
    // Deallocate Aov name
    void free();
    
private:
    // Resolution, X & Y
    int mXres, mYres;
    
    // Bucket origin X and Y, Width, Height
    int mBucket_xo,
        mBucket_yo,
        mBucket_size_x,
        mBucket_size_y;
;
    
    // Sample Per Pixel
    int mSpp;
    
    // Memory
    long long mRam;
    
    // Time
    unsigned int mTime;
    
    // AOV Name
    const char *mAovName;
    
    // Our pixel data pointer (for driver-owned pixels)
    float *mpData;
    
    // Our persistent pixel storage (for Data-owned pixels)
    std::vector<float> mPixelStore;
};



// Used to send an image to a Server
// The Client class is created each time an application wants to send
// an image to the Server. Once it is instantiated the application should
// call open_image(), send_pixels(), and close_image() to send an image to the Server
class Client
{
    friend class Server;
public:
    // Creates a new Client object and tell it to connect any messages to
    // the specified host/port
    Client(std::string hostname, int port);
    
    ~Client();
    
    // Sends a message to the Server to open a new image
    // The header parameter is used to tell the Server the size of image
    // buffer to allocate.
    void open_image(DataHeader& header);
    
    // Sends a section of image data to the Server
    // Once an image is open a Client can use this to send a series of
    // pixel blocks to the Server. The Data object passed must correctly
    // specify the block position and dimensions as well as provide a
    // pointer to pixel data.
    void send_pixels(DataPixels& data);
    
    // Sends a message to the Server that the Clients has finished
    // This tells the Server that a Client has finished sending pixel
    // information for an image.
    void close_image();
    
    bool connected() { return mIsConnected; }

private:
    void connect();
    void disconnect();
    void quit();
    
    // Store the port we should connect to
    std::string mHost;
    int mPort, mImageId;
    bool mIsConnected;
    
    // TCP stuff
    boost::asio::io_service mIoService;
    boost::asio::ip::tcp::socket mSocket;
};

#endif // ATON_CLIENT_H_
