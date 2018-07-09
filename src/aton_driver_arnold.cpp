/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
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

struct ShaderData
{
    Client* client;
    int index, xres, yres, min_x, min_y, max_x, max_y;
};

node_parameters
{
    AiParameterStr("host", get_host().c_str());
    AiParameterInt("port", get_port());
    AiParameterStr("output", "");
    AiParameterStr("user_intput", "");
    
#ifdef ARNOLD_5
    AiMetaDataSetStr(nentry, NULL, "maya.translator", "aton");
    AiMetaDataSetStr(nentry, NULL, "maya.attr_prefix", "");
    AiMetaDataSetBool(nentry, NULL, "display_driver", true);
    AiMetaDataSetBool(nentry, NULL, "single_layer_driver", false);
#else
    AiMetaDataSetStr(mds, NULL, "maya.translator", "aton");
    AiMetaDataSetStr(mds, NULL, "maya.attr_prefix", "");
    AiMetaDataSetBool(mds, NULL, "display_driver", true);
    AiMetaDataSetBool(mds, NULL, "single_layer_driver", false);
#endif
}

node_initialize
{
    ShaderData* data = (ShaderData*)AiMalloc(sizeof(ShaderData));
    data->client = NULL;
    data->index = get_unique_id();

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
    const int xres = AiNodeGetInt(options, "xres");
    const int yres = AiNodeGetInt(options, "yres");
    
    // Get Regions
    const int min_x = AiNodeGetInt(options, "region_min_x");
    const int min_y = AiNodeGetInt(options, "region_min_y");
    const int max_x = AiNodeGetInt(options, "region_max_x");
    const int max_y = AiNodeGetInt(options, "region_max_y");

    // Set Resolution
    data->min_x = (min_x == INT_MIN) ? 0 : min_x;
    data->min_y = (min_y == INT_MIN) ? 0 : min_y;
    data->max_x = (max_x == INT_MIN) ? 0 : max_x;
    data->max_y = (max_y == INT_MIN) ? 0 : max_y;

    data->xres = calc_res(xres, data->min_x, data->max_x);
    data->yres = calc_res(yres, data->min_y, data->max_y);
    
    // Get Region Area
    const long long region_area = data->xres * data->yres;
    
    // Get Arnold version
    char arch[3], major[3], minor[3], fix[3];
    AiGetVersion(arch, major, minor, fix);
    const int version = pack_4_int(atoi(arch), atoi(major), atoi(minor), atoi(fix));
        
    // Get Frame
    const float frame = AiNodeGetFlt(options, "frame");
    
    // Get Camera Field of view
    AtNode* camera = (AtNode*)AiNodeGetPtr(options, "camera");
    const float cam_fov = AiNodeGetFlt(camera, "fov");
    
    // Get Camera Matrix
#ifdef ARNOLD_5
    const AtMatrix& cMat = AiNodeGetMatrix(camera, "matrix");
#else
    AtMatrix cMat;
    AiNodeGetMatrix(camera, "matrix", cMat);
#endif
    
    const float cam_matrix[16] = {cMat[0][0], cMat[1][0], cMat[2][0], cMat[3][0],
                                  cMat[0][1], cMat[1][1], cMat[2][1], cMat[3][1],
                                  cMat[0][2], cMat[1][2], cMat[2][2], cMat[3][2],
                                  cMat[0][3], cMat[1][3], cMat[2][3], cMat[3][3]};
    
    // Get Samples
    const int& aa_samples = AiNodeGetInt(options, "AA_samples");
    const int& diffuse_samples = AiNodeGetInt(options, "GI_diffuse_samples");
    const int& spec_samples = AiNodeGetInt(options, "GI_specular_samples");
    const int& trans_samples = AiNodeGetInt(options, "GI_transmission_samples");
    const int& sss_samples = AiNodeGetInt(options, "GI_sss_samples");
    const int& volume_samples = AiNodeGetInt(options, "GI_volume_samples");
    
    const int samples[6]  = {aa_samples,
                             diffuse_samples,
                             spec_samples,
                             trans_samples,
                             sss_samples,
                             volume_samples};
    
    // Make image header & send to server
    DataHeader dh(data->index,
                  data->xres,
                  data->yres,
                  region_area,
                  version,
                  frame,
                  cam_fov,
                  cam_matrix,
                  samples);

    try // Now we can connect to the server and start rendering
    {
        if (data->client == NULL)
        {
            // Get Host and Port
            const char* host = AiNodeGetStr(node, "host");
            const int port = AiNodeGetInt(node, "port");
            
            if (host_exists(host))
                data->client = new Client(host, port);
        }
        data->client->openImage(dh);
    }
    catch(const std::exception &e)
    {
        const char* err = e.what();
        AiMsgError("ATON | %s", err);
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

    int pixel_type;
    int spp = 0;
    const void* bucket_data;
    const char* aov_name;
    
    if (data->min_x < 0)
        bucket_xo = bucket_xo - data->min_x;
    if (data->min_y < 0)
        bucket_yo = bucket_yo - data->min_y;
    
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
        DataPixels dp(data->xres,
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

        // Send it to the server
        data->client->sendPixels(dp);
    }
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
    data->client->closeImage();
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
