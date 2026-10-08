AVC Project Setup 10/8/2026
---------------------------------------

prerequisites:
- Windows computer
- VS 2022 with UE components installed
- Unreal Engine 5.7.4

1. regenerate .sln by right click ue project > generate visual studio files

2. download external dependency JUCE from  https://juce.com/download/

3. install JUCE to the directory above your project root. 
	mine is at 'D:\_GitHub\JUCE\' when 'D:\_GitHub\AudioVisual_Controller' is my root

4. open projucer, select open project > 'Audio_Visual_Controller\juice_system_listener'

build the project in VS 2022

5. open the ue project in unreal 5.7.4, likely through the epic launcher for versioning
	click yes on rebuild project modules 
	click Platforms> package project > windows > package project
	

6. rebuild and run the project in VS 2022



--------------------------------------
for questions email sophiaboss11@gmail.com, or send a discord request to basevision