/*
Copyright (c) 2019,
Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include <string>
#include <vector>
#include <cstdlib>

#include <ai.h>
#include <stdio.h>

AI_OPERATOR_NODE_EXPORT_METHODS(AtonOperatorMtd);

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

std::vector<std::string> split(std::string str, std::string token)
{
    std::vector<std::string>result;
    while(str.size())
    {
        int index = static_cast<int>(str.find(token));
        
        if(index != std::string::npos)
        {
            result.push_back(str.substr(0, index));
            str = str.substr(index+token.size());
            
            if(str.size() == 0)
                result.push_back(str);
        }
        else
        {
            result.push_back(str);
            str = "";
        }
    }
    return result;
}

struct OpData
{
    AtString driver_name;
};

node_parameters
{
    AiParameterStr("host", get_host().c_str());
    AiParameterInt("port", get_port());
    AiParameterStr("output", "");
    
    AiParameterStr("bucket_scanning", "");
    
    AiParameterInt("region_min_x", 0);
    AiParameterInt("region_min_y", 0);
    AiParameterInt("region_max_x", 0);
    AiParameterInt("region_max_y", 0);
    
    AiParameterBool("ignore_motion_blur", false);
    AiParameterBool("ignore_subdivision", false);
    AiParameterBool("ignore_displacement", false);
    AiParameterBool("ignore_bump", false);
    AiParameterBool("ignore_sss", false);
}

operator_init
{
    OpData* data = (OpData*)AiMalloc(sizeof(OpData));
    
    data->driver_name = AtString("defaultAtonDriver");

    AtNode* driver = AiNode("driver_aton", data->driver_name);
    AiNodeSetStr(driver, "host", AiNodeGetStr(op, "host"));
    AiNodeSetInt(driver, "port", AiNodeGetInt(op, "port"));
    AiNodeSetStr(driver, "output", AiNodeGetStr(op, "output"));

    AiNodeSetLocalData(op, data);
    return true;
}

operator_cook
{
    OpData* data = (OpData*)AiNodeGetLocalData(op);

    AtNode* options = AiUniverseGetOptions();
    AtArray* outputs = AiNodeGetArray(options, "outputs");
    
    int elements = AiArrayGetNumElements(outputs);

    for (int i=0; i<elements; ++i)
    {
        std::string output_string = AiArrayGetStr(outputs, i).c_str();
        std::string name = split(output_string, std::string(" ")).back();
        output_string.replace(output_string.find(name), name.length(), std::string(data->driver_name));
        AiArraySetStr(outputs, i, AtString(output_string.c_str()));
    }
    return true;
}

operator_cleanup
{
    return true;
}

node_loader
{
    if (i>0) return 0;
    node->methods = (AtNodeMethods*)AtonOperatorMtd;
    node->name = "operator_aton";
    node->node_type = AI_NODE_OPERATOR;
    strcpy(node->version, AI_VERSION);
    return true;
}
