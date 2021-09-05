@rem Build project by Visual Studio
copy targgen.VS2017.sln targgen.sln
copy targgen.VS2017.vcxproj targgen.vcxproj
"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\devenv.com" /build Debug targgen.sln
Debug\targgen.exe
