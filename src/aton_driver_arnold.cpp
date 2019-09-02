/*
Copyright (c) 2019,
Dan Bethell, Johannes Saam, Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include <ai.h>
#include "aton_client.h"

AI_DRIVER_NODE_EXPORT_METHODS(AtonDriverMtd);

inline const int calc_res(int res, int min, int max)
{
    if (min < 0 && max >= res)
        res = max - min + 1;
    else if (min > 0 && max >= res)
        res += max - res + 1;
    else if (min < 0 && max < res)
        res -= min;
    return res;
}

inline const long long calc_rarea(int minx,
                                  int maxx,
                                  int miny,
                                  int maxy)
{
    int w = maxx - minx + 1;
    int h = maxy - miny + 1;
    return w * h;
}

struct ShaderData
{
    Client* client;
    long long session;
    int xres, yres, min_x, min_y, max_x, max_y;
};

enum reconnect
{
    disabled = 0,
    once,
    always,
};

node_parameters
{
    AiParameterStr("host", get_host().c_str());
    AiParameterInt("port", get_port());
    AiParameterStr("output", "");
    AiParameterInt("session", 0);
    AiParameterInt("reconnect", reconnect::disabled);
    
#ifdef ARNOLD_5
    AiMetaDataSetStr(nentry, NULL, AtString("maya.translator"), AtString("aton"));
    AiMetaDataSetStr(nentry, NULL, AtString("maya.attr_prefix"), AtString(""));
    AiMetaDataSetBool(nentry, NULL, AtString("display_driver"), true);
    AiMetaDataSetBool(nentry, NULL, AtString("single_layer_driver"), false);
#else
    AiMetaDataSetStr(mds, NULL, AtString("maya.translator"), AtString("aton"));
    AiMetaDataSetStr(mds, NULL, AtString("maya.attr_prefix"), AtString(""));
    AiMetaDataSetBool(mds, NULL, AtString("display_driver"), true);
    AiMetaDataSetBool(mds, NULL, AtString("single_layer_driver"), false);
#endif
}

node_initialize
{
    ShaderData* data = (ShaderData*)AiMalloc(sizeof(ShaderData));
    data->client = NULL;
    
    data->session = AiNodeGetInt(node, AtString("session"));
    if (data->session == 0)
        data->session = get_unique_id();

#ifdef ARNOLD_5
    AiDriverInitialize(node, true);
    AiNodeSetLocalData(node, data);
#else
    AiDriverInitialize(node, true, data);
#endif
}

node_update {}

driver_supports_pixel_type { return true; }

driver_extension { return NULL; }

driver_open
{
#ifdef ARNOLD_5
    ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);
#else
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
#endif
    
    // Get Options Node
    AtNode* options = AiUniverseGetOptions();
    
    // Get Resolution
    const int xres = AiNodeGetInt(options, AtString("xres"));
    const int yres = AiNodeGetInt(options, AtString("yres"));

    // Get Regions
    const int min_x = AiNodeGetInt(options, AtString("region_min_x"));
    const int min_y = AiNodeGetInt(options, AtString("region_min_y"));
    const int max_x = AiNodeGetInt(options, AtString("region_max_x"));
    const int max_y = AiNodeGetInt(options, AtString("region_max_y"));

    // Set Resolution
    data->min_x = (min_x == INT_MIN) ? 0 : min_x;
    data->min_y = (min_y == INT_MIN) ? 0 : min_y;
    data->max_x = (max_x == INT_MIN) ? 0 : max_x;
    data->max_y = (max_y == INT_MIN) ? 0 : max_y;

    data->xres = calc_res(xres, data->min_x, data->max_x);
    data->yres = calc_res(yres, data->min_y, data->max_y);
    
    // Get Region Area
    const long long region_area = calc_rarea(data_window.minx,
                                             data_window.maxx,
                                             data_window.miny,
                                             data_window.maxy);
    
    // Get Arnold version
    char arch[3], major[3], minor[3], fix[3];
    AiGetVersion(arch, major, minor, fix);
    const int version = pack_4_int(atoi(arch), atoi(major), atoi(minor), atoi(fix));
        
    // Get Frame
    const float frame = AiNodeGetFlt(options, AtString("frame"));
    
    // Get Camera Field of view
    AtNode* camera = (AtNode*)AiNodeGetPtr(options, AtString("camera"));
    const float cam_fov = AiNodeGetFlt(camera, AtString("fov"));
    
    // Get Camera Matrix
#ifdef ARNOLD_5
    const AtMatrix& cMat = AiNodeGetMatrix(camera, AtString("matrix"));
#else
    AtMatrix cMat;
    AiNodeGetMatrix(camera, "matrix", cMat);
#endif
    
    const float cam_matrix[16] = {cMat[0][0], cMat[1][0], cMat[2][0], cMat[3][0],
                                  cMat[0][1], cMat[1][1], cMat[2][1], cMat[3][1],
                                  cMat[0][2], cMat[1][2], cMat[2][2], cMat[3][2],
                                  cMat[0][3], cMat[1][3], cMat[2][3], cMat[3][3]};
    
    // Get Samples
    const int& aa_samples = AiNodeGetInt(options, AtString("AA_samples"));
    const int& diffuse_samples = AiNodeGetInt(options, AtString("GI_diffuse_samples"));
    const int& spec_samples = AiNodeGetInt(options, AtString("GI_specular_samples"));
    const int& trans_samples = AiNodeGetInt(options, AtString("GI_transmission_samples"));
    const int& sss_samples = AiNodeGetInt(options, AtString("GI_sss_samples"));
    const int& volume_samples = AiNodeGetInt(options, AtString("GI_volume_samples"));
    
    // Get Pixel Aspect Ratio
    const float pixel_aspect = AiNodeGetFlt(options, AtString("pixel_aspect_ratio"));
    
    const int samples[6]  = {aa_samples,
                             diffuse_samples,
                             spec_samples,
                             trans_samples,
                             sss_samples,
                             volume_samples};

    const char* output = AiNodeGetStr(node, AtString("output"));
    
    // Make image header & send to server
    DataHeader dh(data->session,
                  data->xres,
                  data->yres,
                  pixel_aspect,
                  region_area,
                  version,
                  frame,
                  cam_fov,
                  cam_matrix,
                  samples,
                  output);

    // Get Host and Port
    const char* host = AiNodeGetStr(node, AtString("host"));
    const int port = AiNodeGetInt(node, AtString("port"));
    
    if (data->client == NULL)
        data->client = new Client(host, port);
    
    try
    {
        data->client->send_header(dh);
    }
    catch(const std::exception &e)
    {
        const char* err = e.what();
        AiMsgError("ATON | Host %s with Port %i was not found! %s", host, port, err);
    }
}

driver_needs_bucket { return true; }

driver_prepare_bucket {}

driver_process_bucket {}

driver_write_bucket
{
    
#ifdef ARNOLD_5
    ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);
#else
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
#endif

    int pixel_type, spp = 0;
    const void* bucket_data;
    const char* aov_name;
    const int reconnect_mode = AiNodeGetInt(node, AtString("reconnect"));

    if (data->min_x < 0)
        bucket_xo = bucket_xo - data->min_x;
    if (data->min_y < 0)
        bucket_yo = bucket_yo - data->min_y;
    
    // Reconnect to server
    if (reconnect_mode)
        data->client->connect();
        
    while (AiOutputIteratorGetNext(iterator, &aov_name, &pixel_type, &bucket_data))
    {
        const float* ptr = reinterpret_cast<const float*>(bucket_data);
        const long long memory = AiMsgUtilGetUsedMemory();
        const unsigned int time = AiMsgUtilGetElapsedTime();
        
        switch (pixel_type)
        {
            case(AI_TYPE_INT):
            case(AI_TYPE_UINT):
            case(AI_TYPE_FLOAT):
                spp = 1;
                break;
            case(AI_TYPE_RGBA):
                spp = 4;
                break;
            default:
                spp = 3;
        }
        
        // Create our DataPixels object
        DataPixels dp(data->session,
                      data->xres,
                      data->yres,
                      bucket_xo,
                      bucket_yo,
                      bucket_size_x,
                      bucket_size_y,
                      spp,
                      memory,
                      time,
                      aov_name,
                      ptr);

        data->client->send_pixels(dp);
    }
    
    // Disconnect from server
    if (reconnect_mode == reconnect::always)
        data->client->disconnect();
}

driver_close {}

node_finish
{
// Release the driver
#ifdef ARNOLD_5
    ShaderData* data = (ShaderData*)AiNodeGetLocalData(node);
#else
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
#endif
    
    delete data->client;
    AiFree(data);

#ifndef ARNOLD_5
    AiDriverDestroy(node);
#endif
}

node_loader
{
    sprintf(node->version, AI_VERSION);

    switch (i)
    {
        case 0:
            node->methods = (AtNodeMethods*) AtonDriverMtd;
            node->output_type = AI_TYPE_RGBA;
            node->name = "driver_aton";
            node->node_type = AI_NODE_DRIVER;
            break;
        default:
        return false;
    }
    return true;
}
