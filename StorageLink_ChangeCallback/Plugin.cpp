/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 * Copyright (C) 2022 Martin Krämer, University Hospital Jena, Germany
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include <orthanc/OrthancCPlugin.h>
#include "../../orthanc/OrthancServer/Plugins/Samples/Common/OrthancPluginCppWrapper.h"
#include "../../orthanc/OrthancFramework/Sources/Logging.h"

#include <string.h>
#include <stdio.h>
#include <string>
#include <filesystem>

static OrthancPluginContext* context = NULL;
std::string storage_directory;
std::string link_directory;

OrthancPluginErrorCode OnStoredCallback(const OrthancPluginDicomInstance* instance,
                                        const char* instanceId)
{
  char info[1024];

  // Get DICOM Header for that instance and retrieve PatientID, StudyInstanceUID, SeriesInstanceUID and SOPInstanceUID
  OrthancPlugins::OrthancString s;
  Json::Value json;
  s.Assign(OrthancPluginGetInstanceJson(context, instance));
  //OrthancPluginLogWarning(context, s.GetContent());
  s.ToJson(json);
  std::string patientID = json["0010,0020"]["Value"].asString();
  std::string studyUID = json["0020,000d"]["Value"].asString();
  std::string seriesUID = json["0020,000e"]["Value"].asString();
  std::string sopUID = json["0008,0018"]["Value"].asString();
  if ((patientID.empty()) || (studyUID.empty()) || (seriesUID.empty()) || (sopUID.empty())) {
    sprintf(info, "Failed to get instance UIDs for instance %s", instanceId);
    OrthancPluginLogError(context, info);
    return OrthancPluginErrorCode_Success;
  }

  // Build target path
  std::string target = link_directory + std::filesystem::path::preferred_separator +
                       patientID + std::filesystem::path::preferred_separator +
                       studyUID + std::filesystem::path::preferred_separator +
                       seriesUID + std::filesystem::path::preferred_separator;

  // Query to retrieve FileUuid of that instance    
  std::string fileuuid;
  OrthancPluginMemoryBuffer tmp;  
  memset(&tmp, 0, sizeof(tmp));
  sprintf(info, "/instances/%s/attachments/dicom/info", instanceId);
  if (OrthancPluginRestApiGet(context, &tmp, info) == OrthancPluginErrorCode_Success)
  {
    std::string rest_result;
    Json::Value json_rest_result;
    rest_result.assign(reinterpret_cast<const char*>(tmp.data), tmp.size);
    OrthancPlugins::ReadJson(json_rest_result, rest_result);
    fileuuid = json_rest_result["Uuid"].asString();
    OrthancPluginFreeMemoryBuffer(context, &tmp);
  } else {
    sprintf(info, "Failed to retrieve attachmend uuid for instance %s", instanceId);
    OrthancPluginLogError(context, info);
    return OrthancPluginErrorCode_Success; // return sucess nevertheless, link creation is only secondary
  }

  // Build source path
  std::string source = storage_directory + std::filesystem::path::preferred_separator +
                       std::string(&fileuuid[0], &fileuuid[2]) + std::filesystem::path::preferred_separator +
                       std::string(&fileuuid[2], &fileuuid[4]) + std::filesystem::path::preferred_separator + 
                       fileuuid;  
  
  //OrthancPluginLogWarning(context, instanceId);
  //OrthancPluginLogWarning(context, source.c_str());
  //OrthancPluginLogWarning(context, (target + sopUID).c_str());

  // Create output directory and symbolic link
  if (!std::filesystem::exists(target + sopUID)) {
    std::filesystem::create_directories(target);
    std::filesystem::create_symlink(source, target + sopUID);
  }

  return OrthancPluginErrorCode_Success;
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context = c;
    OrthancPluginLogWarning(context, "Storage Link plugin is initializing");
    OrthancPlugins::SetGlobalContext(context);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }
    OrthancPluginLogWarning(context, "Reading config");

    // get orthanc storage directory
    OrthancPlugins::OrthancConfiguration configuration;    
    if (configuration.LookupStringValue(storage_directory, "StorageDirectory")) {
      OrthancPluginLogWarning(context, ("Using base storage directory: " + storage_directory).c_str());
    } else {
      OrthancPluginLogWarning(context, "No StorageDirectory configuration set");
      return 0;
    }    

  	// get custom link directory
    if (!configuration.IsSection("StorageLink"))
    {
      OrthancPluginLogWarning(context, "No available configuration for the StorageLink storage area plugin");
      return 0;
    }
    OrthancPlugins::OrthancConfiguration storageLink;
    configuration.GetSection(storageLink, "StorageLink");    
    if (storageLink.LookupStringValue(link_directory, "LinkDirectory")) {
      OrthancPluginLogWarning(context, ("Using base storage link directory: " + link_directory).c_str());
    } else {
      OrthancPluginLogWarning(context, "No LinkDirectory configuration set");
      return 0;
    }

    OrthancPluginRegisterOnStoredInstanceCallback(context, OnStoredCallback);

    OrthancPluginLogWarning(context, "Done Init");
    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPluginLogWarning(context, "Storage Link plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "Storage Link";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "0.1";
  }
}
