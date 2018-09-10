/*
Copyright (c) 2018,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include "aton_server.h"
#include "aton_client.h"
#include <boost/lexical_cast.hpp>

using namespace boost::asio;

Server::Server(): mPort(0),
                  mSocket(mIoService),
                  mAcceptor(mIoService)
{
}

Server::Server(int port): mPort(0),
                          mSocket(mIoService),
                          mAcceptor(mIoService)
{
    connect(port);
}

Server::~Server()
{
    if (mAcceptor.is_open())
        mAcceptor.close();
}

void Server::connect(int port, bool search)
{
    // Disconnect if necessary
    if (mAcceptor.is_open())
        mAcceptor.close();

    // Reconnect at specified port
    int start_port = port;
    while (!mAcceptor.is_open() && port < start_port + 99)
    {
        try
        {
            using boost::asio::ip::tcp;
            tcp::endpoint endpoint(ip::tcp::v4(), port);
            mAcceptor.open(endpoint.protocol());
            mAcceptor.set_option(ip::tcp::acceptor::reuse_address(false));
            mAcceptor.bind(endpoint);
            mAcceptor.listen();
            mPort = port;
        }
        catch (...)
        {
            mAcceptor.close();
            if (!search)
                break;
            else
                port++;
        }
    }

    // Handle failed connection
    if (!mAcceptor.is_open())
    {
        char buffer[32];
        sprintf(buffer, "port: %d", start_port);
        if (search)
            sprintf(buffer, "port: %d-%d", start_port, start_port + 99);
        std::string error = "Failed to connect to port ";
        error += buffer;
        throw std::runtime_error( error.c_str() );
    }
}

void Server::quit()
{
    std::string hostname("localhost");
    Client client(hostname, mPort);
    client.quit();
}

void Server::accept()
{
    if (mSocket.is_open())
        mSocket.close();
    mAcceptor.accept(mSocket);
}

int Server::listen_type()
{
    int type;
    
    try
    {
        read(mSocket, buffer(reinterpret_cast<char*>(&type), sizeof(int)));
    
        if (type == 2 || type == 9)
        {
            mSocket.close();
            if (type == 9)
                mAcceptor.close();
        }
    }
    catch( ... )
    {
        mSocket.close();
        throw std::runtime_error("Could not read from socket!");
    }
    
    return type;
}

DataHeader Server::listenHeader()
{
    DataHeader dh;

    // Send back an image id
    int image_id = 1;
    write(mSocket, buffer(reinterpret_cast<char*>(&image_id), sizeof(int)));
    
    // Read data from the buffer
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mSession), sizeof(long long)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mXres), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mYres), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mPixAspectRatio), sizeof(float)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mRArea), sizeof(long long)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mVersion), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mFrame), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mCamFov), sizeof(float)));
    
    const int camMatrixSize = 16;
    dh.mCamMatrixStore.resize(camMatrixSize);
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mCamMatrixStore[0]), sizeof(float)*camMatrixSize));

    const int samplesSize = 6;
    dh.mSamplesStore.resize(samplesSize);
    read(mSocket, buffer(reinterpret_cast<char*>(&dh.mSamplesStore[0]), sizeof(int)*samplesSize));
    
    // Get output size
    size_t output_size;
    read(mSocket, buffer(reinterpret_cast<char*>(&output_size), sizeof(size_t)));
    
    // Get output name
    char* output_name = new char[output_size];
    read(mSocket, buffer(output_name, output_size));
    dh.mOutputName = output_name;

    return dh;
}

DataPixels Server::listenPixels()
{
    DataPixels dp;
    
    // Receive image id
    int image_id;
    read(mSocket, buffer(reinterpret_cast<char*>(&image_id), sizeof(int)) );

    // Read data from the buffer
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mXres), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mYres), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mBucket_xo), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mBucket_yo), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mBucket_size_x), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mBucket_size_y), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mSpp), sizeof(int)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mRam), sizeof(long long)));
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mTime), sizeof(int)));

    // Get aov name's size
    size_t aov_size;
    read(mSocket, buffer(reinterpret_cast<char*>(&aov_size), sizeof(size_t)));

    // Get aov name
    char* aov_name = new char[aov_size];
    read(mSocket, buffer(aov_name, aov_size));
    dp.mAovName = aov_name;

    // Get pixels
    const int num_samples = dp.bucket_size_x() * dp.bucket_size_y() * dp.spp();
    dp.mPixelStore.resize(num_samples);
    read(mSocket, buffer(reinterpret_cast<char*>(&dp.mPixelStore[0]), sizeof(float)*num_samples));
    return dp;
}

