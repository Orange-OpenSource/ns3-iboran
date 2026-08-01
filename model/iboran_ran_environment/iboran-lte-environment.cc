/*
 * Software Name: ib-ORAN
 * SPDX-FileCopyrightText: Copyright (c) Orange SA
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This software is derived from ns-3, which is distributed
 * under the GNU General Public License v2.0.
 * See https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * Authors: see CONTRIBUTORS.md
 */

#include "../iboran_application/iboran-application.h"
#include "iboran-ran-common.h"
#include "iboran-ran-params.h"

#include <ns3/applications-module.h>
#include <ns3/buildings-module.h>
#include <ns3/config-store.h>
#include <ns3/core-module.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/internet-module.h>
#include <ns3/lte-module.h>
#include <ns3/mobility-module.h>
#include <ns3/netanim-module.h>
#include <ns3/network-module.h>
#include <ns3/point-to-point-module.h>
#include <ns3/trace-fading-loss-model.h>

#include <filesystem>
#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IbOranLteEnvironment");

// == LTE-SPECIFIC SETUP FUNCTIONS ==============================================
void
SetupLteRadioLink(const CommonRanParams& commonParams,
                  const LteParams& lteParams,
                  Ptr<LteHelper> lteHelper,
                  NodeContainer& enbNodes,
                  NodeContainer& ueNodes)
{
    // -- LARGE-SCALE FADING ----------------------------------------------------
    NodeContainer allNodes;
    allNodes.Add(enbNodes);
    allNodes.Add(ueNodes);

    // install buildings for shadowing and propagation
    BuildingsHelper::Install(allNodes);
    lteHelper->SetSpectrumChannelType("ns3::MultiModelSpectrumChannel");
    /**
     * "MultiModelSpectrumChannel":
     * Supports multiple propagation models simultaneously,
     * allowing accurate per-link pathloss and shadowing computations.
     * It is required for HybridBuildingsPropagationLossModel and FDD operation
     * because UL and DL frequencies can be different, and each link may experience
     * different indoor/outdoor conditions, wall penetration, and fading.
     */

    // pathloss and shadowing
    lteHelper->SetAttribute("PathlossModel",
                            StringValue("ns3::HybridBuildingsPropagationLossModel"));
    lteHelper->SetPathlossModelAttribute("ShadowSigmaOutdoor", DoubleValue(7.0)); // [dB]
    lteHelper->SetPathlossModelAttribute("ShadowSigmaIndoor", DoubleValue(3.0));
    lteHelper->SetPathlossModelAttribute("ShadowSigmaExtWalls", DoubleValue(5.0));
    lteHelper->SetPathlossModelAttribute("Environment", StringValue("Urban"));
    /* Options: "Urban", "Suburban", "Rural" */
    lteHelper->SetPathlossModelAttribute("CitySize", StringValue("Medium"));
    /* Options: "Small", "Medium", "Large" */

    // -- SMALL-SCALE FADING ----------------------------------------------------
    lteHelper->SetAttribute("FadingModel", StringValue("ns3::TraceFadingLossModel"));

    // load 3GPP trace file: EPA model at 3 km/h
    std::ifstream ifTraceFile;
    ifTraceFile.open("../../src/lte/model/fading-traces/fading_trace_EPA_3kmph.fad",
                     std::ifstream::in);
    if (ifTraceFile.good())
    {
        lteHelper->SetFadingModelAttribute(
            "TraceFilename",
            StringValue("../../src/lte/model/fading-traces/fading_trace_EPA_3kmph.fad"));
    }
    else
    {
        lteHelper->SetFadingModelAttribute(
            "TraceFilename",
            StringValue("src/lte/model/fading-traces/fading_trace_EPA_3kmph.fad"));
    }

    // default values for trace file
    lteHelper->SetFadingModelAttribute("TraceLength", TimeValue(Seconds(10.0)));
    lteHelper->SetFadingModelAttribute("WindowSize", TimeValue(Seconds(0.5)));
    lteHelper->SetFadingModelAttribute("RbNum", UintegerValue(lteParams.dlRbNum));

    // -- RADIO LINK CHARACTRISTICS ---------------------------------------------
    lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(lteParams.dlRbNum));
    lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(lteParams.ulRbNum));
    lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(lteParams.dlEarfcn));
    lteHelper->SetEnbDeviceAttribute("UlEarfcn", UintegerValue(lteParams.ulEarfcn));

    std::cout << std::setw(30) << "LTE radio link setup" << ": OK" << std::endl;
}

void
SetupLteProtocolStack(CommonRanParams& commonParams, LteParams& lteParams, Ptr<LteHelper> lteHelper)
{
    // -- RRC LAYER -------------------------------------------------------------
    // configure downlink transmission mode
    Config::SetDefault("ns3::LteEnbRrc::DefaultTransmissionMode",
                       UintegerValue(lteParams.transmissionMode));

    lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
    /**
     * Commonly used LTE handover algorithms in ns-3:
     * "ns3::NoOpHandoverAlgorithm" -> no handovers
     * "ns3::A3RsrpHandoverAlgorithm" -> RSRP-based A3 event
     * "ns3::A3RsrqHandoverAlgorithm" -> RSRQ-based A3 event
     *
     * Notes:
     * - A3 event is triggered when a neighbor cell becomes offset better than
     * the serving cell for a certain time-to-trigger duration (default 256 ms).
     * - Hysteresis avoids ping-pong effects due to signal fluctuations.
     * - Serving cell threshold avoids handovers to very weak cells.
     * - RSRP-based handover is based on signal strength (power).
     * - RSRQ-based handover is based on signal quality (interference + noise).
     * - RSRP-based handover is more commonly used in practice.
     */

    // to avoid ping-pong effects due to signal fluctuations
    lteHelper->SetHandoverAlgorithmAttribute("Hysteresis",
                                             DoubleValue(commonParams.handoverHysteresis));
    lteHelper->SetHandoverAlgorithmAttribute(
        "TimeToTrigger",
        TimeValue(MilliSeconds(commonParams.handoverTimeToTrigger)));

    // -- MAC AND RLC LAYERs ----------------------------------------------------
    // set MAC scheduler mechanism
    lteHelper->SetSchedulerType("ns3::IbOranLteMacScheduler");

    // enable or disable HARQ for eNodeBs
    Config::SetDefault("ns3::RrFfMacScheduler::HarqEnabled", BooleanValue(true));
    Config::SetDefault("ns3::IbOranLteMacScheduler::HarqEnabled", BooleanValue(true));

    // -- PHY LAYER -------------------------------------------------------------
    // configure power control mechanism
    Config::SetDefault("ns3::LteUePhy::EnableUplinkPowerControl",
                       BooleanValue(true) // automatically enables open-loop power control
    );
    Config::SetDefault("ns3::LteUePowerControl::ClosedLoop",
                       BooleanValue(true) // combined with closed-loop control
    );
    Config::SetDefault("ns3::LteUePowerControl::AccumulationEnabled", BooleanValue(true));
    Config::SetDefault("ns3::LteUePowerControl::Alpha",
                       DoubleValue(commonParams.compensationFactor));
    Config::SetDefault("ns3::LteUePowerControl::PoNominalPusch",
                       IntegerValue(lteParams.p0NominalPusch));
    Config::SetDefault("ns3::LteUePowerControl::Pcmax", DoubleValue(commonParams.ueMaxTxPower));
    Config::SetDefault("ns3::LteUePowerControl::Pcmin", DoubleValue(commonParams.ueMinTxPower));
    Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(commonParams.ueMaxTxPower));
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(commonParams.xnbTxPower));

    std::cout << std::setw(30) << "LTE protocol stack setup" << ": OK" << std::endl;
}

// == MAIN SIMULATOR ============================================================
int
main(int argc, char* argv[])
{
    SimParams& params = SimParams::GetInstance();
    params.SetRatType(RatType::LTE);

    std::filesystem::path current_dir = std::filesystem::current_path();
    params.LoadFromYaml((current_dir / "contrib/iboran/notebook/logs/runConfig.yaml").string());

    params.Print(std::cout);

    auto& commonParams = params.GetCommonRanParams();
    auto& lteParams = params.GetLteParams();

    // -- LTE AND EPC INITIALIZATION --------------------------------------------
    /**
     * Topology:
     * [UE] <-LTE interface-> [eNodeB] <-(S1 link)-> [SGW/PGW]
     *  <-P2P backhaul-> [Remote Host]
     */

    // create LTE helper to configure UEs, eNodeBs, and radio access
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // create EPC helper for LTE core network (PGW/SGW/MME) and UEs connectivity
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // -- BACKHAUL SETUP --------------------------------------------------------
    std::cout << "\n-- Backhaul Setup ---------------------" << std::endl;

    // create the PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // create the SGW and MME nodes (for now, just to set mobility modes)
    Ptr<Node> sgw = epcHelper->GetSgwNode();
    Ptr<Node> mme = NodeList::GetNode(2); // internally created as 3rd node

    // create the Remote Host (internet server)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(commonParams.remoteHostNum);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // setup backhaul nodes mobility
    SetupBackhaulMobility(commonParams, pgw, sgw, mme, remoteHost);

    std::cout << std::left << std::setw(30) << "PGW ID" << ": " << pgw->GetId() << "\n"
              << std::setw(30) << "SGW ID" << ": " << sgw->GetId() << "\n"
              << std::setw(30) << "MME ID" << ": " << mme->GetId() << "\n"
              << std::setw(30) << "Remote Host ID" << ": " << remoteHost->GetId() << std::endl;

    // create p2p backhaul link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(commonParams.backhaulDataRate));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(commonParams.backhaulMtu));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(commonParams.backhaulDelay)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    std::cout << std::setw(30) << "Ethernet link setup" << ": OK" << std::endl;

    // assign IP address
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address pgwAddr = internetIpIfaces.GetAddress(0 // PGW interface = 0 -> IP: 1.0.0.1
    );
    Ipv4Address remoteHostAddr =
        internetIpIfaces.GetAddress(1 // RemoteHost interface = 1 -> IP: 1.0.0.2
        );

    std::cout << std::setw(30) << "PGW IP address" << ": " << pgwAddr << "\n"
              << std::setw(30) << "Remote Host IP address" << ": " << remoteHostAddr << std::endl;

    // set up the static routing for the Remote Host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->SetDefaultRoute(pgwAddr, 1 // via PGW (Remote Host interface = 1)
    );

    std::cout << std::setw(30) << "Static routing setup" << ": OK" << std::endl;

    // -- LTE FRONTHAUL SETUP --------------------------------------------------
    std::cout << "\n-- LTE Fronthaul Setup ----------------" << std::endl;

    // define nodes configuration
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(commonParams.xnbNum);
    ueNodes.Create(commonParams.ueNum);

    std::cout << std::left << std::setw(30) << "eNodeB ID" << ": " << enbNodes.Get(0)->GetId()
              << "\n";
    for (uint32_t ueCount = 0; ueCount < ueNodes.GetN(); ++ueCount)
    {
        Ptr<Node> ue = ueNodes.Get(ueCount);
        std::stringstream labelStream;
        labelStream << "UE " << ueCount + 1 << " ID";
        std::cout << std::setw(30) << labelStream.str() << ": " << ue->GetId() << "\n";
    }

    // setup fronthaul nodes mobility
    SetupFronthaulMobility(commonParams, params.GetRatType(), enbNodes, ueNodes);

    // setup radio link (fading models and link characteristics)
    SetupLteRadioLink(commonParams, lteParams, lteHelper, enbNodes, ueNodes);

    // setup the LTE protocol stack (PHY, MAC, RLC, RRC layers)
    SetupLteProtocolStack(commonParams, lteParams, lteHelper);

    // create devices and install them in the nodes
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    std::cout << std::setw(30) << "Node device setup" << ": OK" << std::endl;

    // assign IP address to UEs and set the default gateway
    std::cout << std::setw(30) << "Gateway IP address" << ": " << pgwAddr << std::endl;
    for (uint32_t ueCount = 0; ueCount < ueNodes.GetN(); ++ueCount)
    {
        Ptr<Node> ue = ueNodes.Get(ueCount);
        // set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1
                                         // via LTE interface (index 1) to EPC default gateway
        );
        std::stringstream labelStream;
        labelStream << "UE " << ueCount + 1 << " IP address";
        std::cout << std::setw(30) << labelStream.str() << ": " << ueIpIfaces.GetAddress(ueCount)
                  << "\n";
    }

    // attach all UEs to the primary eNodeB
    for (uint32_t ueCount = 0; ueCount < ueDevs.GetN(); ++ueCount)
    {
        lteHelper->Attach(ueDevs.Get(ueCount), enbDevs.Get(0));
    }

    std::cout << std::setw(30) << "UE attachement to eNodeB" << ": OK" << std::endl;

    // -- BEARER SETUP ---------------------------------------------------------
    uint8_t qci = GetQciForDataFormat(commonParams.dataFormat);
    EpsBearer bearer;

    if (IsGbrBearer(qci))
    {
        GbrQosInformation gbrQos;
        gbrQos.gbrDl = commonParams.dlBitRate;
        gbrQos.gbrUl = commonParams.ulBitRate;
        gbrQos.mbrDl = commonParams.dlBitRate * 1.2;
        gbrQos.mbrUl = commonParams.ulBitRate * 1.2;
        bearer = EpsBearer(static_cast<EpsBearer::Qci>(qci), gbrQos);
    }
    else
    {
        bearer = EpsBearer(static_cast<EpsBearer::Qci>(qci));
    }

    // activate for all UEs
    for (uint32_t ueCount = 0; ueCount < ueNodes.GetN(); ++ueCount)
    {
        lteHelper->ActivateDedicatedEpsBearer(ueDevs.Get(ueCount), bearer, EpcTft::Default());
    }

    std::cout << std::setw(30) << "Bearer setup" << ": OK" << std::endl;

    // -- APPLICATION SETUP ----------------------------------------------------
    ApplicationContainer txApps;
    ApplicationContainer rxApps;
    SetupIbOranApplications(commonParams,
                            params.GetRatType(),
                            ueNodes,
                            remoteHost,
                            ueIpIfaces,
                            remoteHostAddr,
                            txApps,
                            rxApps,
                            ueDevs);

    std::cout << std::setw(30) << "Application setup" << ": OK" << std::endl;

    // -- LOGGING & TRACING SETUP ----------------------------------------------
    std::cout << "\n-- Tracing & Logging ------------------" << std::endl;

    if (commonParams.enableIbOranSchedCoreLog)
    {
        LogComponentEnable("IbOranSchedCore", LOG_LEVEL_INFO);
    }

    if (commonParams.enableRrcLog)
    {
        LogComponentEnable("LteUeRrc", LOG_LEVEL_INFO);
        LogComponentEnable("LteEnbRrc", LOG_LEVEL_INFO);
    }

    lteHelper->EnableTraces();

    // flow monitor setup
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // netanim setup
    AnimationInterface* anim = SetupNetAnim(commonParams,
                                            params.GetRatType(),
                                            remoteHost,
                                            pgw,
                                            sgw,
                                            mme,
                                            enbNodes,
                                            ueNodes,
                                            "iboran-lte-anim.xml");

    // -- SIMULATION RUN -------------------------------------------------------
    std::cout << "ib-ORAN LTE Environment: Running..." << std::endl;
    Simulator::Stop(Seconds(commonParams.simTime));
    Simulator::Run();

    PrintApplicationStats(commonParams, params.GetRatType(), ueNodes, txApps, rxApps);

    // save flow monitor stats
    std::filesystem::path stats_dir = current_dir / "contrib/iboran/notebook/.flowstats";
    std::filesystem::create_directories(stats_dir);
    std::filesystem::path stats_path = stats_dir / "iboran-flowstats.xml";
    flowMonitor->SerializeToXmlFile(stats_path.string(), true, true);

    std::cout << "\nib-ORAN LTE Environment: Run finished.\n";

    delete anim;
    anim = nullptr;

    Simulator::Destroy();

    return 0;

} // end main
