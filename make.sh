# command line argument $1: file path
mkdir -p bin # create directory for executable
rm -f ./bin/PTXprofiler.exe # prevent execution of old version if compiling fails
g++ ./src/*.cpp -o ./bin/PTXprofiler.exe
./bin/PTXprofiler.exe $1
