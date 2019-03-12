/*
Copyright (c) 2019,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef ATON_SERVER_H_
#define ATON_SERVER_H_

#include "aton_client.h"
#include <boost/asio.hpp>

 // Represents a listening Server, ready to accept incoming images
 // This class wraps up the provision of a TCP port, and handles incoming
 // connections from Client objects when they're ready to send image data
class Server
{
public:
    // Creates a new server. By default the Server is not connected at creation time
    Server();
    
    // Creates a new server and calls connect() with the specified port number
    Server(int port);
    
    // Shuts down the server, closing any open ports if the server is connected
    ~Server();

    // If true is passed as the second parameter then the server will
    // search for the first available port if the specified one is not
    // available. To find out which port the server managed to connect to,
    // call get_port() afterwards
    void connect(int port, bool search=false);
    
    // Sets up the server to accept an incoming Client connections.
    void accept();

    // This function blocks (and so may be require running on a separate thread),
    // returning once a Client has sent a message.
    // The returned Data object is filled with the relevant information and
    // passed back ready for handling by the parent application
    int listen_type();
    DataHeader listenHeader();
    DataPixels listenPixels();
    
    // This can be used to exit a listening loop running on a separate thread
    void quit();

    // Returns whether or not the server is connected to a port
    bool connected() { return mAcceptor.is_open(); }

    //! Returns the port the server is currently connected to
    int get_port() { return mPort; }

private:
    // Port we're listening to
    int mPort;
    
    // TCP stuff
    boost::asio::io_service mIoService;
    boost::asio::ip::tcp::socket mSocket;
    boost::asio::ip::tcp::acceptor mAcceptor;
};

#endif // ATON_SERVER_H_
