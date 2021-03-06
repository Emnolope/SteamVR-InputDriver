//////////////////////////////////////////////////////////////////////////////
// soft_knuckles_debug_handler.cpp
// 
// See header for description
//
#include "dprintf.h"
#include "soft_knuckles_device.h"
#include "soft_knuckles_debug_handler.h"
#include <string.h>

#if defined(_WIN32)
#define strtok_r strtok_s
#pragma warning(disable: 4996)
#endif

namespace soft_knuckles {

SoftKnucklesDebugHandler::SoftKnucklesDebugHandler()
    : m_device(nullptr)
{}

void SoftKnucklesDebugHandler::Init(SoftKnucklesDevice *d)
{
    m_device = d;
}

void SoftKnucklesDebugHandler::SetPosition(double x, double y, double z)
{
    m_device->m_pose.vecPosition[0] = x;
    m_device->m_pose.vecPosition[1] = y;
    m_device->m_pose.vecPosition[2] = z;
}

#if 0
void SoftKnucklesDebugHandler::ClickButton(SoftKnucklesButtons button, float new_state, float time_offset)
{
    vr::EVRInputError error;
    error = vr::VRDriverInput()->UpdateScalarComponent(m_buttons_touch[button], new_state, time_offset);  // disclaimer here about time
    dprintf("button updated result: %d\n", error);
}


void ExecuteConsoleCommand(const vector<string> &command_list, string *response)
{
    m_command.lock();
    *response = "unrecognized command\n";
    if (command_list.size() > 0)
    {
        if (command_list.size() > 1)
        {
            std::vector<SoftKnucklesDevice *> devices;
            std::string targets;
            if (command_list[0] == "l")
            {
                devices.push_back(&m_knuckles[0]);
                targets = "l";
            }
            else if (command_list[0] == "r")
            {
                devices.push_back(&m_knuckles[1]);
                targets = "r";
            }
            else if (command_list[0] == "*")
            {
                devices.push_back(&m_knuckles[0]);
                devices.push_back(&m_knuckles[1]);
                targets = "*";
            }

            if (command_list[1] == "1") // activate controllers
            {
                for (auto device : devices)
                {
                    vr::VRServerDriverHost()->TrackedDeviceAdded(
                        device->get_serial().c_str(),
                        TrackedDeviceClass_Controller,
                        device);
                }
                *response = targets + " activated\n";
            }
            else if (command_list[1] == "p" && command_list.size() == 5) // position controller. e.g. l p 1,2,3
            {
                double x = atof(command_list[2].c_str());
                double y = atof(command_list[3].c_str());
                double z = atof(command_list[4].c_str());
                for (auto device : devices)
                {
                    device->SetPosition(x, y, z);
                }
                *response = targets + " moved\n";
            }
            else if (
                command_list[1] == "a" ||
                command_list[1] == "b" ||
                command_list[1] == "s" ||
                command_list[1] == "m"
                &&  command_list.size() == 3) // activate button e.g. l a 1
            {
                bool button_state = (command_list[2] == "1" || command_list[2] == "T" || command_list[2] == "t");
                SoftKnucklesButtons button;
                switch (command_list[1][0])
                {
                case 'a': button = BUTTON_A; break;
                case 'b': button = BUTTON_B; break;
                case 's': button = BUTTON_SYSTEM; break;
                case 'm': button = BUTTON_APPLICATION; break;
                }

                for (auto device : devices)
                {
                    device->TouchButton(button, button_state, 0);
                }
                if (button_state)
                {
                    *response = targets + " button " + command_list[1] + " pressed\n";
                }
                else
                {
                    *response = targets + "button " + command_list[1] + " released\n";
                }
            }
        }
    }
    m_command.unlock();
}



static void client_thread(KnucklesProvider *pthis, SOCKET my_client)
{
    dprintf("client thread started. %d\n", my_client);
    const char *welcome_message =
        "\n" \
        "*** Welcome to the Soft Knuckles driver.  \n" \
        "Commands:\n" \
        " l 1        : activate left controller\n" \
        " r 1        : activate right controller\n" \
        " l p x y z  : position left controller to (x,y,z) in raw coordinate system\n" \
        " r p x y z  : position right controller to (x,y,z) in raw coordinate system\n" \
        " l a 1      : activate button A on left controller\n"\
        " l a 0      : deactivate button A on left controller\n"\
        "\n";

    sendto(my_client, welcome_message, (int)strlen(welcome_message) + 1, 0, 0, 0);

    vector<string> command_list;
    string response;

    while (1)
    {
        char buf[1025];
        int ret = recv(my_client, buf, 1024, 0);
        dprintf("ret: %d\n", ret);
        if (ret < 0)
        {
            dprintf("client thread got error %d from read\n", ret);
            return;
        }
        else
        {
            buf[ret] = 0;
            tokenize(buf, " \r\t\n,", &command_list);
            pthis->ExecuteConsoleCommand(command_list, &response);
            sendto(my_client, response.c_str(), (int)response.size(), 0, 0, 0);
        }
    }
}

static void listen_thread(KnucklesProvider *pthis)
{
    dprintf("listen thread started\n");
    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        dprintf("socket failed with error: %ld\n", WSAGetLastError());
        return;
    }
    else
    {
        sockaddr_in service;
        service.sin_family = AF_INET;
        service.sin_addr.s_addr = inet_addr("127.0.0.1");
        service.sin_port = htons(27015);

        if (::bind(ListenSocket, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
            dprintf("bind failed with error: %ld\n", WSAGetLastError());
            closesocket(ListenSocket);
            return;
        }

        while (1)
        {
            if (listen(ListenSocket, 1) == SOCKET_ERROR) {
                dprintf("listen failed with error: %ld\n", WSAGetLastError());
                closesocket(ListenSocket);
                return;
            }
            SOCKET AcceptSocket;
            dprintf("Waiting for client to connect...\n");
            AcceptSocket = accept(ListenSocket, NULL, NULL);
            if (AcceptSocket == INVALID_SOCKET) {
                dprintf("accept failed with error: %ld\n", WSAGetLastError());
                closesocket(ListenSocket);
                return;
            }
            else
                dprintf("Client connected. %d\n", AcceptSocket);
            thread client = thread(client_thread, pthis, AcceptSocket);
            client.detach();
        }
    }
}

#endif

static void tokenize(const char *const_input, const char *delim, vector<string> *ret)
{
    char input[1024];
    strcpy(input, const_input);
    ret->resize(0);
    char *context;
    char *token = strtok_r(input, delim, &context);
    while (token)
    {
        ret->push_back(token);
        token = strtok_r(nullptr, delim, &context);
    }
}

static void set_response(const char *response_source, char *response, uint32_t response_buffer_size)
{
    if (response_buffer_size > 1)
    {
        size_t strlen_src = strlen(response_source);
        if (strlen_src + 1 > response_buffer_size)
        {
            memcpy(response, response_source, response_buffer_size - 1);
            response[response_buffer_size - 1] = 0;
        }
        else
        {
            memcpy(response, response_source, strlen_src + 1);
        }
    }
}

void SoftKnucklesDebugHandler::InitializeLookupTable()
{
    m_inputstring2index.reserve(m_device->m_num_component_definitions);
    for (uint32_t i = 0; i < m_device->m_num_component_definitions; i++)
    {
        m_inputstring2index[m_device->m_component_definitions[i].full_path] = i;
    }
}

void SoftKnucklesDebugHandler::DebugRequest(const char *request, char *response, uint32_t response_buffer_size)
{
    if (m_inputstring2index.size() == 0)
        InitializeLookupTable();

    dprintf("device_id %d received request: %s\n", m_device->m_id, request);

    vector<string> tokens;
    tokenize(request, " \r\t\n,", &tokens);
    bool success = false;
    if (tokens.size() > 1) // need at least two params
    {
        if (tokens[0] == "pos")
        {
            // set the position of this controller
            double x = atof(tokens[1].c_str());;
            double y = atof(tokens[2].c_str());;
            double z = atof(tokens[3].c_str());;
            m_device->m_pose.vecPosition[0] = x;
            m_device->m_pose.vecPosition[1] = y;
            m_device->m_pose.vecPosition[2] = z;
            // the controller has an update thread, so it'll get posted on the next update
            success = true;
        }
        else
        {
            // tokens[0] is an input state path
            string input_state_path = tokens[0];
            auto iter = m_inputstring2index.find(input_state_path);
            if (iter != m_inputstring2index.end())
            {
                uint32_t index = (*iter).second;
                VRInputComponentHandle_t component_handle = m_device->m_component_handles[index];
                ComponentType component_type = m_device->m_component_definitions[index].component_type;
                if (component_type == CT_BOOLEAN)
                {
                    bool new_value = (tokens[1] == "1");
                    dprintf("setting %s to %d\n", input_state_path.c_str(), new_value);
                    EVRInputError err = vr::VRDriverInput()->UpdateBooleanComponent(component_handle, new_value, 0);
                    if (err != VRInputError_None)
                    {
                        dprintf("error %d\n", err);
                        success = false;
                    }
                    else
                    {
                        success = true;
                    }
                }
                else if (component_type == CT_SCALAR)
                {
                    float new_value = (float)atof(tokens[1].c_str());
                    dprintf("setting %s to %f\n", input_state_path.c_str(), new_value);
                    EVRInputError err = vr::VRDriverInput()->UpdateScalarComponent(component_handle, new_value, 0);
                    if (err != VRInputError_None)
                    {
                        dprintf("error %d\n", err);
                    }
                    else
                    {
                        success = true;
                    }
                }
            }
            else
            {
                dprintf("could not find component named %s\n", input_state_path.c_str());
            }
        }
    }
    else
    {
        dprintf("not enough tokens: %d\n", tokens.size());
    }

    if (success)
    {
        set_response("ok", response, response_buffer_size);
    }
    else
    {
        set_response("fail", response, response_buffer_size);
    }
}

};
