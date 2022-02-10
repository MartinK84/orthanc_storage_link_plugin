# orthancStorageAreaPlugin
Plugin for the Orthanc DICOM server that creates a symbolic link of each stored instance into a human readable directory structure of:
```
PatientID/
    StudyInstanceUID/
        SeriesInstanceUID/
            SOPInstanceUID

``` 

The repository contains two plugins that try to implement the same functionality:
* **StorageLink_ChangeCallback**: the linking is realized as an OnStoredInstanceCallback(), this plugin is functional and should work - however it has not been tested extensively
* **StorageLink_StorageArea**: this plugin is just a stub and is not functional at the moment. It is at some point in the future supposed to realize the linking in an alternative way as a complete storage area plugin.

## Notes
* Requires C++17 for compilation because the linking is done using std::filesystem.
* Only tested under linux (Cent-OS Stream 9) but should in theory work on any filesystem that supports symoblic links, probably also NTFS under Windows.

## Build
For building the plugin the orthanc source code must be checked out in the parent directory to ../orthanc

Build instructions after cloning the repository
```
mkdir build
cd build
cmake -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Release ../
make
```

## Configuration
Make sure load the plugin in your orthanc configuration and configure the path for the linked dicom files using the ```StorageLink``` settings group:
```
    "Plugins" : [
        ...,
	    "../plugins/libStorageLink_ChangeCallback.so"
    ],
    "StorageLink" : {
          "LinkDirectory": "/storage/orthanc_link"
    }