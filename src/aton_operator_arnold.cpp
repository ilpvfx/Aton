/*
Copyright (c) 2020,
Vahan Sosoyan.
All rights reserved. See COPYING.txt for more details.
*/

#include <string>
#include <vector>
#include <cstdlib>

#include <ai.h>
#include <stdio.h>


#define operator_post_cook \
static bool OperatorPostCook(AtNode* op, void* user_data)


AI_OPERATOR_NODE_EXPORT_METHODS(AtonOperatorMtd);


using namespace std;


const int get_port()
{
    const char* def_port = getenv("ATON_PORT");
    
    if (def_port == NULL)
        return 9201;
    
    return atoi(def_port);
}

const string get_host()
{
    const char* def_host = getenv("ATON_HOST");
    
    if (def_host == NULL)
        return string("127.0.0.1");

    return string(def_host);
}

vector<string> split_str(string str, string token)
{
    vector<string>result;
    while(str.size())
    {
        int index = static_cast<int>(str.find(token));
        
        if(index != string::npos)
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
    AtNode* driver;
};

node_parameters
{
    AiParameterStr("host", get_host().c_str());
    AiParameterInt("port", get_port());
    AiParameterStr("output", "");
    AiParameterInt("session", 0);
    AiParameterInt("reconnect", 0);
    AiParameterBool("keep_existing_outputs", true);
}

operator_init
{
    OpData* data = (OpData*)AiMalloc(sizeof(OpData));
    
    data->driver = AiNode("driver_aton", AtString("defaultAtonDriver"));
    
    AiNodeSetStr(data->driver, "host", AiNodeGetStr(op, "host"));
    AiNodeSetInt(data->driver, "port", AiNodeGetInt(op, "port"));
    AiNodeSetStr(data->driver, "output", AiNodeGetStr(op, "output"));
    AiNodeSetInt(data->driver, "session", AiNodeGetInt(op, "session"));
    AiNodeSetInt(data->driver, "reconnect", AiNodeGetInt(op, "reconnect"));
    AiNodeSetLocalData(op, data);
    
    return true;
}

operator_cook
{
    OpData* data = (OpData*)AiNodeGetLocalData(op);

    AtNode* options = AiUniverseGetOptions();
    AtArray* outputs = AiNodeGetArray(options, "outputs");

    int offset = 0;
    int elements = AiArrayGetNumElements(outputs);
    
    if (AiNodeGetBool(op, "keep_existing_outputs"))
    {
        AiArrayResize(outputs, 2 * elements, 0);
        offset = elements;
    }
    
    for (int i=0; i<elements; ++i)
    {
        string output_string = AiArrayGetStr(outputs, i).c_str();
        string name = split_str(output_string, string(" ")).back();
        output_string.replace(output_string.find(name), name.length(), AiNodeGetStr(data->driver, "name"));

        AiArraySetStr(outputs, i + offset, AtString(output_string.c_str()));
    }

    return true;
}

operator_post_cook
{
    
    return true;
}

operator_cleanup
{
    OpData* data = (OpData*)AiNodeGetLocalData(op);
    AiFree(data);
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
