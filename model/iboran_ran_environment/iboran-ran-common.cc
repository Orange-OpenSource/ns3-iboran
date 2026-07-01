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

#include <iomanip>
#include <random>
#include <sstream>

namespace ns3
{

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
        DoubleValue(10.0) // change direction every 10 m
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
    std::filesystem::path input_dir = current_dir / "contrib/iboran/notebook/dataset";
    std::filesystem::path output_dir = current_dir / "contrib/iboran/notebook/dataset_dscv";
    std::filesystem::create_directories(input_dir);
    std::filesystem::create_directories(output_dir);

    std::random_device rd;
    std::mt19937 gen(rd());
    // random UE object ID (1 ID per input file)
    std::uniform_int_distribution<> dist(1, 5);

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
            // std::string dlUeObjectId = std::to_string(
            //     (ueCount / 2) + 1); // to reduce randomness for deriving final results

            // -- Receiver side (UE)
            Ptr<IbOranReceptionApplication> dlRxApp = CreateObject<IbOranReceptionApplication>();
            dlRxApp->SetLocalAddress(
                InetSocketAddress(ueIpIfaces.GetAddress(ueCount), dlPort + ueCount));
            dlRxApp->SetOutputPath(
                output_dir / ("output_dl_" + dlUeObjectId + "_ueidx_" + std::to_string(ueCount)),
                commonParams.dataFormat);
            dlRxApp->SetDataFormat(commonParams.dataFormat);
            dlRxApp->SetUseUdp(commonParams.dlUseUdp);
            ueNodes.Get(ueCount)->AddApplication(dlRxApp);
            rxApps.Add(dlRxApp);

            // -- Transmitter side (Remote Host)
            Ptr<IbOranTransmissionApplication> dlTxApp =
                CreateObject<IbOranTransmissionApplication>();
            dlTxApp->LoadData(input_dir / ("input_dl_" + dlUeObjectId), commonParams.dataFormat);
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

            // -- Receiver side (Remote Host)
            Ptr<IbOranReceptionApplication> ulRxApp = CreateObject<IbOranReceptionApplication>();
            ulRxApp->SetLocalAddress(InetSocketAddress(remoteHostAddr, ulPort + ueCount));
            ulRxApp->SetOutputPath(
                output_dir / ("output_ul_" + ulUeObjectId + "_ueidx_" + std::to_string(ueCount)),
                commonParams.dataFormat);
            ulRxApp->SetDataFormat(commonParams.dataFormat);
            ulRxApp->SetUseUdp(commonParams.ulUseUdp);
            remoteHost->AddApplication(ulRxApp);
            rxApps.Add(ulRxApp);

            // -- Transmitter side (UE)
            Ptr<IbOranTransmissionApplication> ulTxApp =
                CreateObject<IbOranTransmissionApplication>();
            ulTxApp->LoadData(input_dir / ("input_ul_" + ulUeObjectId), commonParams.dataFormat);
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
