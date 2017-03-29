// ImGui - standalone example application for Glfw + OpenGL 3, using programmable pipeline
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
#include "stdafx.h"
#include "GL\glew.h"

#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"
#include <stdio.h>
#include <vector>
#include <GLFW/glfw3.h>
#include "LogWindow.h"
#include "ShaderOps.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <unordered_map>
#include <map>
#include <chrono>

#include "NSLoader.h"
#include "NSLoader_Internal.h"
#include <iostream>
#include "AreaFlags.h"
#include <thread>
using namespace nsvr;
static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

bool isValidQuaternion(NSVR_Quaternion& q) {
	return !(std::abs(q.x) < 0.001 
		&& std::abs(q.y) < 0.001
		&& std::abs(q.z) < 0.001
		&& std::abs(q.w) < 0.001);
}


static void ShowHelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(450.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}


void buzz(NSVR_System* system, uint32_t area) {

	using namespace std::chrono;



	NSVR_EventList* events = NSVR_EventList_Create();
	NSVR_Event* basicEvent = NSVR_Event_Create(NSVR_EventType::BASIC_HAPTIC_EVENT);

	//One call in wrapper
	NSVR_Event_SetFloat(basicEvent, "duration", 0.3f);
	NSVR_Event_SetFloat(basicEvent, "strength", 1.0f);
	NSVR_Event_SetInteger(basicEvent, "area", area);
	NSVR_Event_SetInteger(basicEvent, "effect", 666); //doom_buzz
	NSVR_Event_SetFloat(basicEvent, "time", 0.0f);

	NSVR_EventList_AddEvent(events, basicEvent);

	uint32_t handle = NSVR_System_GenerateHandle(system);


	NSVR_EventList_Bind(system, events, handle);


	NSVR_Event_Release(basicEvent);
	NSVR_EventList_Release(events);

	NSVR_System_DoHandleCommand(system, handle, NSVR_HandleCommand::PLAY);

}
class ValueGraph {
public:
	void Render() {
		for (int i = 0; i < NUM_SAMPLES-1; i++) {
			if (i == 0) {
				continue;
			}
			values[i] = values[i + 1];

		}
	
		ImGui::PlotLines(_name.c_str(), values, NUM_SAMPLES, 0, 0, -1, 1.0, ImVec2(300, 40));
	}
	ValueGraph(std::string name) :_name(name) {};
	~ValueGraph() {
		std::fill(values, values + NUM_SAMPLES, 0);
	}
	void AddValue(float f) {
		
		values[NUM_SAMPLES-1] = f;
	}
private:
	const static int NUM_SAMPLES = 2000;
	std::string _name;
	float values[NUM_SAMPLES];

};
class QuaternionDisplay {
public:
	void Render() {
		_xGraph.Render();
		_yGraph.Render();
		_zGraph.Render();
		_wGraph.Render();
	}
	QuaternionDisplay() :_xGraph("X"), _yGraph("Y"), _zGraph("Z"), _wGraph("W") {}
	~QuaternionDisplay() {}
	void Update(float x, float y, float z, float w) {
		_xGraph.AddValue(x);
		_yGraph.AddValue(y);
		_zGraph.AddValue(z);
		_wGraph.AddValue(w);
	}
private:
	ValueGraph _xGraph;
	ValueGraph _yGraph;
	ValueGraph _zGraph;
	ValueGraph _wGraph;
};

int main(int, char**)
{
	// Setup window
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		return 1;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	//if we use 3.3, we need to use shaders to render anything..
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Hardlight Diagnostics", NULL, NULL);
	glfwMakeContextCurrent(window);
	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
	}
	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	// Setup ImGui binding
	ImGui_ImplGlfwGL3_Init(window, true);

	GLuint programID = LoadShaders("VertexShader.txt", "FragmentShader.txt");

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	bool show_test_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImColor(114, 144, 154);

	static bool show_app_log = true;
	static Log log;

	const char* areas[4] = { "Chest_Left", "Chest_Right", "Upper_Ab_Left", "Upper_Ab_Right" };
	const char* ids[4] = { "0x01", "0x23", "0x42", "0x12" };
	const int statuses[4] = { 1,1,0, 1 };
	// Main loop
	QuaternionDisplay display_chest;
	QuaternionDisplay display_leftUpperArm;
	QuaternionDisplay display_rightUpperArm;

	bool _suitConnected = false;
	std::map<std::string, AreaFlag> padToAreaFlag = {
		{ "B2L", AreaFlag::Back_Left },

		{ "A1L", AreaFlag::Shoulder_Left },
		{ "A2L", AreaFlag::Upper_Arm_Left },
		{ "A3L", AreaFlag::Forearm_Left },
		{ "A1R", AreaFlag::Shoulder_Right } ,
		{ "A2R", AreaFlag::Upper_Arm_Right },
		{ "A3R", AreaFlag::Forearm_Right },
		{ "C1R", AreaFlag::Chest_Right },
		{ "C2R", AreaFlag::Upper_Ab_Right },
		{ "C3R", AreaFlag::Mid_Ab_Right },
		{ "C4R", AreaFlag::Lower_Ab_Right },
		{ "B2R", AreaFlag::Back_Right },

		{ "C1L", AreaFlag::Chest_Left },
		{ "C2L", AreaFlag::Upper_Ab_Left },
		{ "C3L", AreaFlag::Mid_Ab_Left },
		{ "C4L", AreaFlag::Lower_Ab_Left }
	


	};
	

	
	//Let's build the test effect. We'll iterate through all the pads, and then create an event for each.
	
	std::vector<std::string> pads = { "B2R", "A1R", "C1R", "C2R", "C1L", "C2L", "B2L", "A1L" , "A2R", "A3R", "C3R", "C4R", "C3L", "C4L", "A2L", "A3L"};
	std::vector<AreaFlag> order = {
		AreaFlag::Forearm_Left,
		AreaFlag::Upper_Arm_Left,
		AreaFlag::Shoulder_Left,
		AreaFlag::Back_Left,

		AreaFlag::Chest_Left,
		AreaFlag::Upper_Ab_Left,
		AreaFlag::Mid_Ab_Left,
		AreaFlag::Lower_Ab_Left,

		AreaFlag::Lower_Ab_Right,
		AreaFlag::Mid_Ab_Right,
		AreaFlag::Upper_Ab_Right,
		AreaFlag::Chest_Right,

		AreaFlag::Back_Right,
		AreaFlag::Shoulder_Right,
		AreaFlag::Upper_Arm_Right,
		AreaFlag::Forearm_Right

	};
	//Instantiate the plugin
	NSVR_System* system = NSVR_System_Create();
	if (!system) {
		std::cout << "Failed to instantiate the NSVR Plugin";
		abort();
	}

	using namespace std::chrono;

	//Create the list to hold our events
	NSVR_EventList* events = NSVR_EventList_Create();
	float offset = 0.0f;
	for (const auto& area : order) {
		NSVR_Event* myEvent = NSVR_Event_Create(NSVR_EventType::BASIC_HAPTIC_EVENT);

		NSVR_Event_SetFloat(myEvent, "duration", 0.7f);
		NSVR_Event_SetFloat(myEvent, "strength", 1.0f);
		NSVR_Event_SetInteger(myEvent, "area", (int) area);
		NSVR_Event_SetInteger(myEvent, "effect", 666); //doom_buzz
		NSVR_Event_SetFloat(myEvent, "time", offset);
		NSVR_EventList_AddEvent(events, myEvent);
		offset += 1.0f;
	}

	unsigned int test_effect_handle = NSVR_System_GenerateHandle(system);
	NSVR_EventList_Bind(system, events, test_effect_handle);
	NSVR_EventList_Release(events);

	NSVR_Event* superStrong = NSVR_Event_Create(NSVR_EventType::BASIC_HAPTIC_EVENT);
	NSVR_Event_SetFloat(superStrong, "duration", 99999999.0f);
	NSVR_Event_SetInteger(superStrong, "area", (int) AreaFlag::All_Areas);
	NSVR_Event_SetInteger(superStrong, "effect", NSVR_Effect::Hum);

	NSVR_EventList* superStrongList = NSVR_EventList_Create();
	NSVR_EventList_AddEvent(superStrongList, superStrong);

	unsigned int super_strong_handle = NSVR_System_GenerateHandle(system);
	NSVR_EventList_Bind(system, superStrongList, super_strong_handle);
	NSVR_EventList_Release(superStrongList);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
       
		ImGui_ImplGlfwGL3_NewFrame();

        // 1. Show a simple window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
		{
			#pragma region Status Window

			NSVR_System_Status status = { 0 };
			NSVR_System_PollStatus(system, &status);


			ImGui::Begin("Status");
			{
				
				ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
				ImGui::BeginChild("EngineStatus", ImVec2(0, 50), true);
				ImGui::Text("Service connection status"); ImGui::SameLine();
				ShowHelpMarker("The NullSpace VR Runtime Service must be running to use the suit. Check for the small NullSpace icon in the task bar, and make sure that it is green. If it is not green, right click it and hit 'Enable Suit'. ");
				if (status.ConnectedToService) {
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");

				}
				else {
					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Disconnected");

				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
				
			}
			{
				if (status.ConnectedToService) {
					ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
					ImGui::BeginChild("SuitStatus", ImVec2(0, 50), true);
					ImGui::Text("Suit status");
					if (status.ConnectedToSuit) {
						_suitConnected = true;
						ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Plugged in");
					}
					else {
						_suitConnected = false;
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Unplugged");
					}
					ImGui::EndChild();
					ImGui::PopStyleVar();
				}
			
			}
			ImGui::End();
			#pragma endregion

			#pragma region Log Window
			NSVR_LogEntry entry = { 0 };
			if (NSVR_System_PollLogs(system, &entry) == 2) {
				std::string m(entry.Message);
				m.append("\n");
				log.AddLog(m.c_str());
			}
			if (show_app_log) { ShowLog(log, &show_app_log); }
			#pragma endregion
			//ImGui::ShowMetricsWindow();
			#pragma region Motor Diagnostics
			ImGui::Begin("Motors");
			{
				ImGui::Columns(4, "mycolumns4", false);
				ImGui::Separator();
				ImGui::Text("ID"); ImGui::NextColumn();
				ImGui::Text("Name"); ImGui::NextColumn();
				ImGui::Text("Status"); ImGui::NextColumn();
				ImGui::Text("Operation"); ImGui::NextColumn();
				ImGui::Separator();
				for (int i = 0; i < 4; i++) {
					//ImGui::NextColumn();
					ImGui::Text(ids[i]); ImGui::NextColumn();
					ImGui::Text(areas[i]); ImGui::NextColumn();
				
						if (statuses[i]) {
							ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Nominal");
							ImGui::NextColumn();
							
								ImGui::Button("Get Info"); 
						
							ImGui::NextColumn();
						}
						else {
							ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Broken");
							ImGui::NextColumn();
							
								ImGui::Button("Diagnose");
						
							ImGui::NextColumn();
						}
					
				

				}
				ImGui::Columns(1);
				ImGui::Separator();

			}
			ImGui::End();
			#pragma endregion

			#pragma region Motor Tests
			ImGui::Begin("Tests");
			{
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
					ImGui::BeginChild("Warning", ImVec2(0, 30), true);

					ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning!"); ImGui::SameLine();
					ImGui::Text("Do not run multiple tests at the same time");
					ImGui::EndChild();
					ImGui::PopStyleVar();
				}

				{
					ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
					ImGui::BeginChild("Sequential", ImVec2(0, 70), true);
					ImGui::Text("Test all pads sequentially"); ImGui::SameLine(); 
					ShowHelpMarker("The suit will play a strong buzz on each pad, beginning on the left forearm and traversing up to the back, then down to the bottom left ab, then up to the right back, and finally down to the right forearm."); 
					ImGui::NewLine();
					if (ImGui::Button("Start")) {
						NSVR_System_DoHandleCommand(system, test_effect_handle, NSVR_HandleCommand::PLAY);
					}
					ImGui::SameLine();
					if (ImGui::Button("Stop")) {
						NSVR_System_DoHandleCommand(system, test_effect_handle, NSVR_HandleCommand::RESET);
					}
				
					ImGui::EndChild();
					ImGui::PopStyleVar();
				}
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
					ImGui::BeginChild("FullPower", ImVec2(0, 70), true);
					ImGui::Text("Activate entire suit at specified power."); ImGui::NewLine();
					static float f1 = 1.0f;
					static bool _isEffectPlaying;
					bool wasSliderMoved = ImGui::SliderFloat("", &f1, 0.0f, 1.0f, "strength = %.1f");
					ImGui::SameLine(); if (wasSliderMoved) {
						{
							if (!wasSliderMoved) {
								f1 = 10.0f;
							}
							NSVR_System_DoHandleCommand(system, super_strong_handle, NSVR_HandleCommand::RESET);
					
							NSVR_Event* superStrong = NSVR_Event_Create(NSVR_EventType::BASIC_HAPTIC_EVENT);
							NSVR_Event_SetFloat(superStrong, "duration", 99999999.0f);
							NSVR_Event_SetInteger(superStrong, "area", (int)AreaFlag::All_Areas);

							NSVR_Event_SetFloat(superStrong, "strength", f1);

						
							NSVR_Event_SetInteger(superStrong, "effect", NSVR_Effect::Hum);
							
							
							NSVR_EventList* superStrongList = NSVR_EventList_Create();
							NSVR_EventList_AddEvent(superStrongList, superStrong);
							NSVR_EventList_Bind(system, superStrongList, super_strong_handle);
							NSVR_Event_Release(superStrong);
							NSVR_EventList_Release(superStrongList);

							if (_isEffectPlaying) {
								NSVR_System_DoHandleCommand(system, super_strong_handle, NSVR_HandleCommand::PLAY);
							}

					}
					

					}
					if (ImGui::Button("Start")) {
						NSVR_System_DoHandleCommand(system, super_strong_handle, NSVR_HandleCommand::PLAY);
						_isEffectPlaying = true;
					}
					ImGui::SameLine();
					if (ImGui::Button("Stop")) {
						NSVR_System_DoHandleCommand(system, super_strong_handle, NSVR_HandleCommand::RESET);
						_isEffectPlaying = false;
					}

					ImGui::EndChild();
					ImGui::PopStyleVar();
				}
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
					ImGui::BeginChild("PadByPad", ImVec2(0, 150), true);
					ImGui::Text("Test pad by pad "); ImGui::SameLine();
					ShowHelpMarker("Plays a strong buzz on the selected pad. Buttons are arranged as a bird's eye view of the suit.");
					ImGui::NewLine();
					ImGui::Columns(4);
					
					for (int i = 0; i < pads.size(); i++)
					{
						if (i % 2 == 0 && i != 0) {
							ImGui::NextColumn();
						}
						
						if (ImGui::Button(pads[i].c_str(), ImVec2(-1.0f, 0.0f))) {
							buzz(system, (uint32_t)padToAreaFlag[pads[i].c_str()]);
						}
					}


					ImGui::EndChild();

					ImGui::PopStyleVar();
					

				}
			}
			ImGui::End();
			#pragma endregion

			#pragma region Tracking View 
			ImGui::Begin("Tracking");
			{
				if (ImGui::Button("Enable tracking")) {
					NSVR_System_DoEngineCommand(system, NSVR_EngineCommand::ENABLE_TRACKING);
				}
				ImGui::SameLine();
				if (ImGui::Button("Disable tracking")) {
					NSVR_System_DoEngineCommand(system, NSVR_EngineCommand::DISABLE_TRACKING);
				}
				ImGui::NewLine();

				NSVR_TrackingUpdate tracking = { 0 };
				NSVR_System_PollTracking(system, &tracking);
				if (isValidQuaternion(tracking.chest)) {
					ImGui::Text("Chest IMU");
					display_chest.Update(tracking.chest.x, tracking.chest.y, tracking.chest.z, tracking.chest.w);
					display_chest.Render();
				}
				if (isValidQuaternion(tracking.right_upper_arm)) {
					ImGui::Text("Right Upper Arm IMU");

					display_rightUpperArm.Update(tracking.right_upper_arm.x, tracking.right_upper_arm.y, tracking.right_upper_arm.z, tracking.right_upper_arm.w);
					display_rightUpperArm.Render();
				}
				if (isValidQuaternion(tracking.left_upper_arm)) {
					ImGui::Text("Left Upper Arm IMU");

					display_leftUpperArm.Update(tracking.left_upper_arm.x, tracking.left_upper_arm.y, tracking.left_upper_arm.z, tracking.left_upper_arm.w);
					display_leftUpperArm.Render();
				}
			
		
			}
			ImGui::End();
			#pragma endregion
        }

		
	
		
		// Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
      

		//end custom code
		glUseProgram(0);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		 ImGui::Render();
		


        glfwSwapBuffers(window);

		
    }
	glDeleteProgram(programID);
	glDeleteVertexArrays(1, &VertexArrayID);

	NSVR_System_Release(system);

    // Cleanup
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    return 0;
}

