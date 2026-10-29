/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: MIT
 *
 * This software is distributed under the MIT license,
 * see the "LICENSE" file for more details
 * or https://spdx.org/licenses/MIT.html
 *
 * Authors: see CONTRIBUTORS.md
 */

#include "iboran-ran-common.h"

#include "iboran-ran-params.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <yaml-cpp/yaml.h>

NS_LOG_COMPONENT_DEFINE("IbOranRanCommon");

namespace ns3
{

// == P2CPP PARAMS STRUCTURE ==========================================
Py2CppParams
LoadPy2CppParams(const std::filesystem::path& filepath)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(filepath.string());
    }
    catch (const YAML::BadFile&)
    {
        NS_LOG_ERROR("Could not open unified config: " << filepath);
        return {};
    }
    catch (const YAML::ParserException& e)
    {
        NS_LOG_ERROR("Failed to parse unified config '" << filepath << "': " << e.what());
        return {};
    }

    auto py = root["orchestrator"];
    std::string datasetId = py["dataset_id"] ? py["dataset_id"].as<std::string>() : "";

    auto entry = root["datasets"][datasetId];
    if (!entry)
    {
        NS_LOG_ERROR("No 'datasets' entry for dataset_id='" << datasetId << "' in " << filepath);
        return {};
    }

    Py2CppParams params;
    params.id = datasetId;
    params.repository = entry["repository"] ? entry["repository"].as<std::string>() : "";
    params.inputImageFormat =
        entry["input_image_format"] ? entry["input_image_format"].as<std::string>() : "";
    params.inputPattern = entry["input_pattern"] ? entry["input_pattern"].as<std::string>() : "";
    params.outputPattern = entry["output_pattern"] ? entry["output_pattern"].as<std::string>() : "";
    params.inputFields = entry["input_fields"]
                             ? entry["input_fields"].as<std::vector<std::string>>()
                             : std::vector<std::string>{};
    params.outputFields = entry["output_fields"]
                              ? entry["output_fields"].as<std::vector<std::string>>()
                              : std::vector<std::string>{};

    // NOT dataset-specific: shared across all datasets, from `orchestrator.oid_range`.
    params.oidRange = py["oid_range"] ? py["oid_range"].as<std::vector<int>>() : std::vector<int>{};

    return params;
}

std::string
BuildFromPattern(const std::string& pattern,
                 const std::vector<std::string>& fieldOrder,
                 const std::map<std::string, std::string>& values)
{
    std::string result;
    size_t fieldIdx = 0;
    char buf[64];

    for (size_t i = 0; i < pattern.size(); ++i)
    {
        if (pattern[i] == '%' && i + 1 < pattern.size())
        {
            size_t specStart = i;
            size_t j = i + 1;
            while (j < pattern.size() && isdigit(pattern[j]))
            {
                ++j; // width, e.g. "04"
            }
            if (j < pattern.size() && (pattern[j] == 'd' || pattern[j] == 's'))
            {
                std::string spec = pattern.substr(specStart, j - specStart + 1);
                const std::string& fieldName = fieldOrder.at(fieldIdx++);
                const std::string& value = values.at(fieldName);

                if (pattern[j] == 'd')
                {
                    snprintf(buf, sizeof(buf), spec.c_str(), std::stoi(value));
                }
                else
                {
                    snprintf(buf, sizeof(buf), spec.c_str(), value.c_str());
                }
                result += buf;
                i = j;
                continue;
            }
        }
        result += pattern[i];
    }
    return result;
}

std::string
ResolveInputFile(const std::filesystem::path& baseDir,
                 const Py2CppParams& params,
                 const std::map<std::string, std::string>& values,
                 std::mt19937& gen)
{
    std::string namePrefix = BuildFromPattern(params.inputPattern, params.inputFields, values);

    if (namePrefix.find('*') == std::string::npos)
    {
        return (baseDir / (namePrefix + params.inputImageFormat)).string();
    }

    std::vector<std::string> matches;
    std::string prefix = namePrefix.substr(0, namePrefix.find('*'));
    for (const auto& entry : std::filesystem::directory_iterator(baseDir))
    {
        std::string filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) == 0 && entry.path().extension() == params.inputImageFormat)
        {
            matches.push_back(filename);
        }
    }
    if (matches.empty())
    {
        throw std::runtime_error("No input file matching '" + prefix + "*' in " + baseDir.string());
    }

    std::uniform_int_distribution<size_t> pickDist(0, matches.size() - 1);
    return (baseDir / matches[pickDist(gen)]).string();
}

std::string
ResolveInputForUe(const std::filesystem::path& baseDir,
                  const Py2CppParams& params,
                  const std::string& preferredObjectId,
                  const std::string& cameraId,
                  std::mt19937& gen,
                  std::string& objectIdOut)
{
    int lo = params.oidRange.size() > 0 ? params.oidRange[0] : 1;
    int hi = params.oidRange.size() > 1 ? params.oidRange[1] : 5;

    std::vector<std::string> candidates = {preferredObjectId};
    std::vector<int> others;
    for (int id = lo; id <= hi; ++id)
    {
        if (std::to_string(id) != preferredObjectId)
        {
            others.push_back(id);
        }
    }
    std::shuffle(others.begin(), others.end(), gen);
    for (int id : others)
    {
        candidates.push_back(std::to_string(id));
    }

    for (const auto& candidateId : candidates)
    {
        std::map<std::string, std::string> values = {
            {"object_id", candidateId},
            {"camera_id", cameraId},
        };
        try
        {
            std::string path = ResolveInputFile(baseDir, params, values, gen);
            objectIdOut = candidateId;
            return path;
        }
        catch (const std::runtime_error&)
        {
            continue; // this identity has no data for this camera; try another
        }
    }

    throw std::runtime_error("No object ID in [" + std::to_string(lo) + ", " + std::to_string(hi) +
                             "] has data for camera_id=" + cameraId + " in " + baseDir.string());
}

std::string
BuildOutputFilename(const Py2CppParams& params, const std::map<std::string, std::string>& values)
{
    return BuildFromPattern(params.outputPattern, params.outputFields, values);
}

void
WriteInputResolutionRecord(const std::filesystem::path& outPath,
                           const std::map<std::string, std::map<uint32_t, std::string>>& record)
{
    std::filesystem::create_directories(outPath.parent_path());

    std::ofstream ofs(outPath);
    if (!ofs.is_open())
    {
        NS_LOG_ERROR("Could not open input resolution record file: " << outPath);
        return;
    }

    ofs << "{\n";
    bool firstAbbr = true;
    for (const auto& [abbr, ueMap] : record)
    {
        if (!firstAbbr)
        {
            ofs << ",\n";
        }
        firstAbbr = false;

        ofs << "  \"" << abbr << "\": {\n";
        bool firstUe = true;
        for (const auto& [ueIdx, filename] : ueMap)
        {
            if (!firstUe)
            {
                ofs << ",\n";
            }
            firstUe = false;
            ofs << "    \"" << ueIdx + 1 << "\": \"" << filename << "\"";
        }
        ofs << "\n  }";
    }
    ofs << "\n}\n";
}

// == MOBILITY FUNCTIONS ==============================================
void
SetupBackhaulMobility(const CommonRanParams& commonParams,
                      Ptr<Node> pgw,
                      Ptr<Node> sgw,
                      Ptr<Node> mme,
                      Ptr<Node> remoteHost)
{
    // PGW at fixed position
    MobilityHelper pgwMobility;
    pgwMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    pgwMobility.Install(pgw);
    pgw->GetObject<MobilityModel>()->SetPosition(
        Vector(commonParams.simAreaLength * 0.8, commonParams.simAreaLength * 0.8, 0.0));

    // SGW at fixed position
    MobilityHelper sgwMobility;
    sgwMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    sgwMobility.Install(sgw);
    sgw->GetObject<MobilityModel>()->SetPosition(
        Vector(commonParams.simAreaLength * 0.65, commonParams.simAreaLength * 0.65, 0.0));

    // MME at fixed position
    MobilityHelper mmeMobility;
    mmeMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mmeMobility.Install(mme);
    mme->GetObject<MobilityModel>()->SetPosition(
        Vector(commonParams.simAreaLength * 0.5, commonParams.simAreaLength * 0.65, 0.0));

    // Remote Host at fixed position
    MobilityHelper remoteHostMobility;
    remoteHostMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    remoteHostMobility.Install(remoteHost);
    remoteHost->GetObject<MobilityModel>()->SetPosition(
        Vector(commonParams.simAreaLength, commonParams.simAreaLength, 0.0));

    std::cout << std::left << std::setw(30) << "Backhaul mobility setup" << ": OK\n"
              << std::setw(30) << "PGW position" << ": "
              << pgw->GetObject<MobilityModel>()->GetPosition() << " m\n"
              << std::setw(30) << "SGW position" << ": "
              << sgw->GetObject<MobilityModel>()->GetPosition() << " m\n"
              << std::setw(30) << "MME position" << ": "
              << mme->GetObject<MobilityModel>()->GetPosition() << " m\n"
              << std::setw(30) << "Remote Host position" << ": "
              << remoteHost->GetObject<MobilityModel>()->GetPosition() << " m\n";
}

void
SetupFronthaulMobility(const CommonRanParams& commonParams,
                       const std::string& ratType,
                       NodeContainer& xnbNodes,
                       NodeContainer& ueNodes)
{
    // Base Station at fixed position
    MobilityHelper bsMobility;
    bsMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    bsMobility.Install(xnbNodes);
    xnbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(
        Vector(commonParams.simAreaLength * 0.5,
               commonParams.simAreaLength * 0.5,
               30.0) // 30 m tower height
    );

    // UEs with random walk mobility
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds",
        RectangleValue(Rectangle(0, commonParams.simAreaLength, 0, commonParams.simAreaLength)),
        "Speed",
        StringValue("ns3::ConstantRandomVariable[Constant=0.83]"), // ~3 km/h
        "Mode",
        StringValue("Distance"),
        "Distance",
        DoubleValue(100.0) // change direction every 100 m
    );
    ueMobility.Install(ueNodes);

    // Set initial random positions for UEs
    Ptr<UniformRandomVariable> randPos = CreateObject<UniformRandomVariable>();
    randPos->SetAttribute("Min", DoubleValue(0.0));
    randPos->SetAttribute("Max", DoubleValue(commonParams.simAreaLength));
    for (uint32_t i = 0; i < commonParams.ueNum; i++)
    {
        double xPos = randPos->GetValue();
        double yPos = randPos->GetValue();
        ueNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(
            Vector(xPos, yPos, 1.5) // 1.5 m user height
        );
    }

    std::stringstream label;
    label << (ratType == RatType::LTE ? "eNodeB" : "gNB") << " position";
    std::cout << std::setw(30) << "Fronthaul mobility setup" << ": OK\n"
              << std::setw(30) << label.str() << ": "
              << xnbNodes.Get(0)->GetObject<MobilityModel>()->GetPosition() << " m\n";

    for (uint32_t i = 0; i < commonParams.ueNum; i++)
    {
        Vector pos = ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        std::stringstream label;
        label << "UE " << (i + 1) << " initial position";
        std::cout << std::setw(30) << label.str() << ": " << std::setprecision(3) << pos << " m\n";
    }
}

// == APP FUNCTIONS ===================================================
void
SetupIbOranApplications(const CommonRanParams& commonParams,
                        const std::string& ratType,
                        NodeContainer& ueNodes,
                        Ptr<Node> remoteHost,
                        const Ipv4InterfaceContainer& ueIpIfaces,
                        Ipv4Address remoteHostAddr,
                        ApplicationContainer& txApps,
                        ApplicationContainer& rxApps,
                        NetDeviceContainer& ueDevs)
{
    uint32_t dlPort = 2000;
    uint32_t ulPort = 4000;

    std::filesystem::path current_dir = std::filesystem::current_path();
    std::filesystem::path config_path = current_dir / "contrib/iboran/notebook/logs/runConfig.yaml";
    Py2CppParams py2CppParams = LoadPy2CppParams(config_path);

    std::filesystem::path input_dir =
        current_dir / "contrib/iboran/notebook/dataset" / py2CppParams.repository;
    std::filesystem::path output_dir =
        current_dir / "contrib/iboran/notebook/dataset_dscv" / py2CppParams.repository;
    std::filesystem::create_directories(input_dir);
    std::filesystem::create_directories(output_dir);

    std::mt19937 gen(commonParams.rngRun);
    // random UE object ID (1 ID per input file)
    std::uniform_int_distribution<> dist(
        py2CppParams.oidRange.size() > 0 ? py2CppParams.oidRange[0] : 1,
        py2CppParams.oidRange.size() > 1 ? py2CppParams.oidRange[1] : 5);

    // Tracks exactly which input file was resolved for each link/UE, keyed by
    // link abbreviation ("dl"/"ul") then UE index, so it can be written out
    // once the loop below completes.
    std::map<std::string, std::map<uint32_t, std::string>> inputResolutionRecord;

    for (uint32_t ueCount = 0; ueCount < ueNodes.GetN(); ++ueCount)
    {
        // Build a RAT-agnostic RNTI getter
        std::function<uint16_t()> rntiGetter;
        if (ratType == RatType::LTE)
        {
            Ptr<LteUeNetDevice> ueDev = DynamicCast<LteUeNetDevice>(ueDevs.Get(ueCount));
            rntiGetter = [ueDev]() { return ueDev->GetRrc()->GetRnti(); };
        }
        else
        {
            Ptr<NrUeNetDevice> ueDev = DynamicCast<NrUeNetDevice>(ueDevs.Get(ueCount));
            rntiGetter = [ueDev]() { return ueDev->GetRrc()->GetRnti(); };
        }

        // -- DOWNLINK FLOW ------------------------------------------
        if (commonParams.enableDlFlow)
        {
            std::string dlUeObjectId = std::to_string(dist(gen));

            // Resolve the input file first. camera_id is fixed per UE
            // (ueCount + 1) since it represents that UE's stable camera
            // assignment; only consumed for datasets whose input_fields
            // actually include it (e.g. market-1501), harmlessly ignored
            // otherwise (e.g. toy). If the drawn identity has no data for
            // this camera, ResolveInputForUe tries other identities in
            // oid_range instead of relaxing the camera -- so dlUeObjectId
            // may be updated here to whichever identity was actually used.
            std::string dlCameraId = std::to_string(ueCount + 1);
            std::string dlResolvedObjectId = dlUeObjectId;
            std::string dlInputFullPath = ResolveInputForUe(input_dir,
                                                            py2CppParams,
                                                            dlUeObjectId,
                                                            dlCameraId,
                                                            gen,
                                                            dlResolvedObjectId);
            if (dlResolvedObjectId != dlUeObjectId)
            {
                NS_LOG_INFO("UE " << ueCount << " (DL): object_id " << dlUeObjectId
                                  << " has no data for camera_id " << dlCameraId
                                  << "; using object_id " << dlResolvedObjectId << " instead");
                dlUeObjectId = dlResolvedObjectId;
            }
            inputResolutionRecord["dl"][ueCount] =
                std::filesystem::path(dlInputFullPath).filename().string();

            std::string dlInputStem =
                std::filesystem::path(dlInputFullPath).replace_extension("").string();

            // -- Receiver side (UE) -- uses the final (possibly-adjusted)
            // dlUeObjectId so output naming and downstream intent matching
            // stay consistent with the identity actually transmitted.
            std::map<std::string, std::string> dlOutputValues = {
                {"abbr", "dl"},
                {"object_id", dlUeObjectId},
                {"idx", std::to_string(ueCount + 1)},
            };

            Ptr<IbOranReceptionApplication> dlRxApp = CreateObject<IbOranReceptionApplication>();
            dlRxApp->SetLocalAddress(
                InetSocketAddress(ueIpIfaces.GetAddress(ueCount), dlPort + ueCount));
            dlRxApp->SetOutputPath(output_dir / BuildOutputFilename(py2CppParams, dlOutputValues),
                                   commonParams.dataFormat);
            dlRxApp->SetDataFormat(commonParams.dataFormat);
            dlRxApp->SetUseUdp(commonParams.dlUseUdp);
            ueNodes.Get(ueCount)->AddApplication(dlRxApp);
            rxApps.Add(dlRxApp);

            // -- Transmitter side (Remote Host)
            Ptr<IbOranTransmissionApplication> dlTxApp =
                CreateObject<IbOranTransmissionApplication>();
            dlTxApp->SetInputImageFormat(py2CppParams.inputImageFormat);
            dlTxApp->LoadData(dlInputStem, commonParams.dataFormat);
            uint32_t fileSizeBytes = dlTxApp->GetRemainingDataSize();
            if (commonParams.simTime <
                (fileSizeBytes * 8.0 / commonParams.dlBitRate) * commonParams.dlTxRepeat + 5.0)
            {
                std::cerr << "Simulation time may be too short to complete the "
                             "downlink transmission of all data.\n";
            }
            dlTxApp->SetPeer(InetSocketAddress(ueIpIfaces.GetAddress(ueCount), dlPort + ueCount));
            dlTxApp->SetRntiGetter(rntiGetter);
            dlTxApp->SetLinkDirection(LinkDirection::DOWNLINK);
            dlTxApp->SetTxRepeat(commonParams.dlTxRepeat);
            dlTxApp->SetPacketSize(commonParams.dlPacketSize);
            dlTxApp->SetBitRate(commonParams.dlBitRate);
            dlTxApp->SetMinPacketInterval(commonParams.dlMinInterval);
            dlTxApp->SetUseUdp(commonParams.dlUseUdp);
            dlTxApp->SetUeObjectId(dlUeObjectId);
            remoteHost->AddApplication(dlTxApp);
            txApps.Add(dlTxApp);
        }

        // -- UPLINK FLOW --------------------------------------------
        if (commonParams.enableUlFlow)
        {
            std::string ulUeObjectId = std::to_string(dist(gen));
            // std::string ulUeObjectId = std::to_string((ueCount / 2) + 1);

            // Resolve the input file first (see downlink note above); this
            // may adjust ulUeObjectId if the drawn identity has no data for
            // this UE's fixed camera assignment.
            std::string ulCameraId = std::to_string(ueCount + 1);
            std::string ulResolvedObjectId = ulUeObjectId;
            std::string ulInputFullPath = ResolveInputForUe(input_dir,
                                                            py2CppParams,
                                                            ulUeObjectId,
                                                            ulCameraId,
                                                            gen,
                                                            ulResolvedObjectId);
            if (ulResolvedObjectId != ulUeObjectId)
            {
                NS_LOG_INFO("UE " << ueCount << " (UL): object_id " << ulUeObjectId
                                  << " has no data for camera_id " << ulCameraId
                                  << "; using object_id " << ulResolvedObjectId << " instead");
                ulUeObjectId = ulResolvedObjectId;
            }
            inputResolutionRecord["ul"][ueCount] =
                std::filesystem::path(ulInputFullPath).filename().string();
            std::string ulInputStem =
                std::filesystem::path(ulInputFullPath).replace_extension("").string();

            std::map<std::string, std::string> ulOutputValues = {
                {"abbr", "ul"},
                {"object_id", ulUeObjectId},
                {"idx", std::to_string(ueCount + 1)},
            };

            // -- Receiver side (Remote Host)
            Ptr<IbOranReceptionApplication> ulRxApp = CreateObject<IbOranReceptionApplication>();
            ulRxApp->SetLocalAddress(InetSocketAddress(remoteHostAddr, ulPort + ueCount));
            ulRxApp->SetOutputPath(output_dir / BuildOutputFilename(py2CppParams, ulOutputValues),
                                   commonParams.dataFormat);
            ulRxApp->SetDataFormat(commonParams.dataFormat);
            ulRxApp->SetUseUdp(commonParams.ulUseUdp);
            remoteHost->AddApplication(ulRxApp);
            rxApps.Add(ulRxApp);

            // -- Transmitter side (UE)
            Ptr<IbOranTransmissionApplication> ulTxApp =
                CreateObject<IbOranTransmissionApplication>();
            ulTxApp->SetInputImageFormat(py2CppParams.inputImageFormat);
            ulTxApp->LoadData(ulInputStem, commonParams.dataFormat);
            uint32_t fileSizeBytes = ulTxApp->GetRemainingDataSize();
            if (commonParams.simTime <
                (fileSizeBytes * 8.0 / commonParams.ulBitRate) * commonParams.ulTxRepeat + 5.0)
            {
                std::cerr << "Simulation time may be too short to complete the "
                             "uplink transmission of all data.\n";
            }
            ulTxApp->SetPeer(InetSocketAddress(remoteHostAddr, ulPort + ueCount));
            ulTxApp->SetRntiGetter(rntiGetter);
            ulTxApp->SetLinkDirection(LinkDirection::UPLINK);
            ulTxApp->SetTxRepeat(commonParams.ulTxRepeat);
            ulTxApp->SetPacketSize(commonParams.ulPacketSize);
            ulTxApp->SetBitRate(commonParams.ulBitRate);
            ulTxApp->SetMinPacketInterval(commonParams.ulMinInterval);
            ulTxApp->SetUseUdp(commonParams.ulUseUdp);
            ulTxApp->SetUeObjectId(ulUeObjectId);
            ueNodes.Get(ueCount)->AddApplication(ulTxApp);
            txApps.Add(ulTxApp);
        }
    }

    std::filesystem::path input_resolution_path =
        current_dir / "contrib/iboran/notebook/logs/oidQueries.json";
    WriteInputResolutionRecord(input_resolution_path, inputResolutionRecord);

    rxApps.Start(Seconds(1.0));
    txApps.Start(Seconds(1.1));
    rxApps.Stop(Seconds(commonParams.simTime) - Seconds(1.0));
    txApps.Stop(Seconds(commonParams.simTime) - Seconds(1.1));
}

void
PrintApplicationStats(const CommonRanParams& commonParams,
                      const std::string& ratType,
                      NodeContainer& ueNodes,
                      ApplicationContainer& txApps,
                      ApplicationContainer& rxApps)
{
    Time dlAppDuration = Seconds(0.0);
    Time ulAppDuration = Seconds(0.0);

    std::cout << "\nib-ORAN " << ratType << " Environment: Deriving stats..." << std::endl;

    for (uint32_t ueCount = 0; ueCount < ueNodes.GetN(); ++ueCount)
    {
        // -- DOWNLINK STATS -----------------------------------------
        if (commonParams.enableDlFlow)
        {
            uint32_t txIndex = ueCount;
            uint32_t rxIndex = ueCount;

            uint32_t dlPacketTransmitted = 0;
            uint32_t dlPacketReceived = 0;
            Time dlTransmissionDuration = Seconds(0.0);

            Ptr<IbOranTransmissionApplication> dlTxApp =
                DynamicCast<IbOranTransmissionApplication>(txApps.Get(txIndex));
            Ptr<IbOranReceptionApplication> dlRxApp =
                DynamicCast<IbOranReceptionApplication>(rxApps.Get(rxIndex));

            if (dlTxApp)
            {
                uint32_t bytesTransmitted = dlTxApp->GetTotalBytesTransmitted();
                dlPacketTransmitted =
                    (bytesTransmitted + commonParams.dlPacketSize - 1) / commonParams.dlPacketSize;
            }
            if (dlRxApp)
            {
                dlPacketReceived = dlRxApp->GetPacketCount();
                dlTransmissionDuration = dlRxApp->GetTransmissionDuration();
                dlAppDuration += dlTransmissionDuration;
            }

            int32_t dlPacketLost = int32_t(dlPacketTransmitted) - int32_t(dlPacketReceived);
            if (dlPacketLost < 0)
            {
                dlPacketLost = 0;
            }

            if (dlPacketTransmitted > 0)
            {
                std::string color = (dlPacketLost == 0) ? "\033[92m" : "\033[91m";
                std::cout << color << "UE " << std::setw(2) << ueCount
                          << " | Downlink | Succeeded: " << std::setw(4) << dlPacketReceived
                          << "| Lost: " << std::setw(3) << dlPacketLost
                          << " | Duration: " << dlTransmissionDuration.GetMilliSeconds() << "ms"
                          << "\033[0m\n";
            }
        }

        // -- UPLINK STATS -------------------------------------------
        if (commonParams.enableUlFlow)
        {
            uint32_t txIndex = ueNodes.GetN() + ueCount;
            uint32_t rxIndex = ueNodes.GetN() + ueCount;

            uint32_t ulPacketTransmitted = 0;
            uint32_t ulPacketReceived = 0;
            Time ulTransmissionDuration = Seconds(0.0);

            Ptr<IbOranTransmissionApplication> ulTxApp =
                DynamicCast<IbOranTransmissionApplication>(txApps.Get(txIndex));
            Ptr<IbOranReceptionApplication> ulRxApp =
                DynamicCast<IbOranReceptionApplication>(rxApps.Get(rxIndex));

            if (ulTxApp)
            {
                uint32_t bytesTransmitted = ulTxApp->GetTotalBytesTransmitted();
                ulPacketTransmitted =
                    (bytesTransmitted + commonParams.ulPacketSize - 1) / commonParams.ulPacketSize;
            }
            if (ulRxApp)
            {
                ulPacketReceived = ulRxApp->GetPacketCount();
                ulTransmissionDuration = ulRxApp->GetTransmissionDuration();
                ulAppDuration += ulTransmissionDuration;
            }

            int32_t ulPacketLost = int32_t(ulPacketTransmitted) - int32_t(ulPacketReceived);
            if (ulPacketLost < 0)
            {
                ulPacketLost = 0;
            }

            if (ulPacketTransmitted > 0)
            {
                std::string color = (ulPacketLost == 0) ? "\033[92m" : "\033[91m";
                std::cout << color << "UE " << std::setw(2) << ueCount
                          << " | Uplink   | Succeeded: " << std::setw(4) << ulPacketReceived
                          << "| Lost: " << std::setw(3) << ulPacketLost
                          << " | Duration: " << ulTransmissionDuration.GetMilliSeconds() << "ms"
                          << "\033[0m\n";
            }
        }
    }
    std::cout << std::endl;
    std::cout << "ib-ORAN " << ratType << " Environment: Downlink flow finished in "
              << dlAppDuration.GetSeconds() << "s.\n";
    std::cout << "ib-ORAN " << ratType << " Environment: Uplink flow finished in "
              << ulAppDuration.GetSeconds() << "s.";
}

// == ANIMATION FUNCTIONS =============================================
AnimationInterface*
SetupNetAnim(const CommonRanParams& commonParams,
             const std::string& ratType,
             Ptr<Node> remoteHost,
             Ptr<Node> pgw,
             Ptr<Node> sgw,
             Ptr<Node> mme,
             NodeContainer& xnbNodes,
             NodeContainer& ueNodes,
             const std::string& outputFile)
{
    std::filesystem::path current_dir = std::filesystem::current_path();
    std::filesystem::path anim_dir = current_dir / "contrib/iboran/notebook/netanim";
    std::filesystem::create_directories(anim_dir);
    std::filesystem::path anim_path = anim_dir / outputFile;

    AnimationInterface* anim = new AnimationInterface(anim_path.string());

    anim->UpdateNodeDescription(remoteHost, "Remote Host");
    anim->UpdateNodeColor(remoteHost, 187, 187, 187);
    anim->UpdateNodeSize(remoteHost->GetId(), 2, 2);

    anim->UpdateNodeDescription(pgw, "PGW");
    anim->UpdateNodeColor(pgw, 255, 255, 255);
    anim->UpdateNodeSize(pgw->GetId(), 1, 1);

    anim->UpdateNodeDescription(sgw, "SGW");
    anim->UpdateNodeColor(sgw, 255, 255, 255);
    anim->UpdateNodeSize(sgw->GetId(), 1, 1);

    anim->UpdateNodeDescription(mme, "MME");
    anim->UpdateNodeColor(mme, 255, 255, 255);
    anim->UpdateNodeSize(mme->GetId(), 1, 1);

    anim->UpdateNodeDescription(xnbNodes.Get(0), (ratType == RatType::LTE ? "eNodeB" : "gNB"));
    anim->UpdateNodeColor(xnbNodes.Get(0), 204, 51, 17);
    anim->UpdateNodeSize(xnbNodes.Get(0)->GetId(), 2, 2);

    for (short ueCount = 0; ueCount < commonParams.ueNum; ueCount++)
    {
        std::string ueNodeStr = "UE " + std::to_string(ueCount + 1);
        anim->UpdateNodeDescription(ueNodes.Get(ueCount), ueNodeStr);
        anim->UpdateNodeColor(ueNodes.Get(ueCount), 0, 119, 187);
        anim->UpdateNodeSize(ueNodes.Get(ueCount)->GetId(), 2, 2);
    }

    anim->EnablePacketMetadata(true);

    std::cout << std::setw(30) << "Animation setup" << ": OK\n";

    return anim;
}

} // namespace ns3
