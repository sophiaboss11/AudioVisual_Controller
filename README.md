AVC Project Setup 10/8/2026

\---------------------------------------



prerequisites:

* Windows computer
* VS 2022 with UE components installed
* Unreal Engine 5.7.4



1. regenerate .sln: right click ue project > generate visual studio files
2. download external dependency JUCE from  https://juce.com/download/
3. install JUCE to the directory above your project root. 
4. &#x09;mine is at 'D:\\\_GitHub\\JUCE\\' when 'D:\\\_GitHub\\AudioVisual\_Controller' is my root
5. build the .sln and project in VS 2022
6. open the ue project file in unreal 5.7.4 editor
7. in ue editor click Platforms> package project > windows > package project > \[select project root]
8. set solution configuration to local windows debugger
9. rebuild and run the project in VS 2022



note: 

* core project files are located at Source/AVC/Private
* play any audio on your computer to watch your scene react



\--------------------------------------

for questions email sophiaboss11@gmail.com, or send a discord request to basevision

