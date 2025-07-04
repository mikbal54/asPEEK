./
 - demo : contains a compiled asPEEK server 
 - example : contains the source code of the demo
 - include : contains the include files needed to use asPEEK. add this include directory to your project
 - src : include cpp files asPEEK uses too construct its .lib/.so files 
 - websocketpp : includes cpp/c files asPEEK uses from websocketpp project
 
 
 How to use:
 
 - Use Cmake to compile asPEEK.
 - Edit CmakeLists.txt files to set AngelScript and Boost paths. Websocket++ sources are already included no need to build them separately.
 - Add ./include directory to your project. And include asPEEK.h
 - Link asPEEK.lib/asPEEK.so to your project
 - ./example contains an an example server that uses asPEEK.
 - Look at https://bitbucket.org/saejox/aspeek for a small tutorial. Also asPEEK.h has comments to help integration
 
 
@@ Muhammed Ikbal Akpaca 2013 @@